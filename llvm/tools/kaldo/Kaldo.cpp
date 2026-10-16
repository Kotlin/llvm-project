#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "Kaldo.hpp"
#include "ObjCInteropManager.hpp"
#include "Utility.hpp"

#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/DebugObjectManagerPlugin.h"
#include "llvm/ExecutionEngine/Orc/Debugging/DebuggerSupport.h"
#include "llvm/ExecutionEngine/Orc/Debugging/DebuggerSupportPlugin.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/JITLinkRedirectableSymbolManager.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/MapperJITLinkMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/MemoryMapper.h"
#include "llvm/ExecutionEngine/Orc/UnwindInfoRegistrationPlugin.h"

#include "llvm/BinaryFormat/Magic.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include "Expected.hpp"
#include "KaldoInternal.hpp"
#include "plugins/KotlinSymbolExternalizer.hpp"
#include "plugins/MachOHostDataSymbolGenerator.hpp"
#include "plugins/ObjCSelectorFixupPlugin.hpp"
#include "plugins/WeakSymbolFallbackGenerator.hpp"

#include "llvm/ADT/ScopeExit.h"

namespace {

// region Constants
constexpr auto KStubsJdName = "Kaldo_stubs";
constexpr auto KBootstrapJdName = "Kaldo_bootstrap";
constexpr auto KReloadJdName = "Kaldo_reload$";

constexpr auto KKonanStartSymbol = "Konan_start";
constexpr auto KKonanConstructorsSymbol = "_Konan_constructors";
// endregion

} // namespace

static std::unique_ptr<llvm::MemoryBuffer>
readObjectFileFromPath(const std::string_view ObjectPath) {
  auto ObjBuffOrErr = llvm::MemoryBuffer::getFile(ObjectPath);
  if (!ObjBuffOrErr) {
    return std::unique_ptr<llvm::MemoryBuffer>(nullptr);
  }
  return std::move(*ObjBuffOrErr);
}

static llvm::Error callKonanConstructors(llvm::orc::ExecutionSession &ES,
                                         const llvm::DataLayout &DL,
                                         llvm::orc::JITDylib &BootJD) {

  auto Mangler = llvm::orc::MangleAndInterner(ES, DL);

  auto ConstrOrErr =
      ES.lookup(llvm::orc::makeJITDylibSearchOrder(
                    {&BootJD}, llvm::orc::JITDylibLookupFlags::MatchAllSymbols),
                Mangler(KKonanConstructorsSymbol));

  if (!ConstrOrErr)
    return ConstrOrErr.takeError();

  auto ConstructorsFn = ConstrOrErr->getAddress().toPtr<void (*)()>();
  llvm::dbgs() << llvm::formatv(
      "[kaldo] :: calling `Konan_constructors@{0:x}` to register InitNodes\n",
      reinterpret_cast<uintptr_t>(ConstructorsFn));

  ConstructorsFn();

  return llvm::Error::success();
}

tl::expected<std::unique_ptr<llvm::kaldo::SymbolsOrchestrator>, std::string>
llvm::kaldo::SymbolsOrchestrator::create(const Config &Config) {
  InitializeNativeTarget();
  // Initialized for debugging reasons
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  InitializeNativeTargetDisassembler();

  auto JTMB = llvm::orc::JITTargetMachineBuilder::detectHost();
  if (!JTMB) {
    return tl::make_unexpected(dumpLlvmErrorToString(JTMB.takeError()));
  }

  std::unique_ptr<SymbolsRedirector> SymRedirector{nullptr};
  std::unique_ptr<objc::ObjCInteropManager> ObjCInteropManager{nullptr};

  auto JITOrErr =
      llvm::orc::LLJITBuilder()
          .setJITTargetMachineBuilder(std::move(*JTMB))
          .setPlatformSetUp(
              llvm::orc::ExecutorNativePlatform(Config.OrcRuntimePath))
          .setObjectLinkingLayerCreator(
              [](llvm::orc::ExecutionSession &ES)
                  -> Expected<std::unique_ptr<llvm::orc::ObjectLayer>> {
                // 1 GB slab: prevents 32-bit delta overflow in __unwind_info.
                constexpr uint64_t SlabSize = 1024ull * 1024 * 1024;
                auto Mapper =
                    llvm::orc::MapperJITLinkMemoryManager::CreateWithMapper<
                        llvm::orc::InProcessMemoryMapper>(SlabSize);
                if (!Mapper) {
                  return Mapper.takeError();
                }

                auto OLL = std::make_unique<llvm::orc::ObjectLinkingLayer>(
                    ES, std::move(*Mapper));
#if defined(__APPLE__)
                OLL->addPlugin(
                    std::make_shared<
                        llvm::kaldo::orc::plugin::ObjCSelectorFixupPlugin>());
#endif
                return std::move(OLL);
              })
          .setNotifyCreatedCallback([&](llvm::orc::LLJIT &J) -> llvm::Error {
            auto &MainJD = J.getMainJITDylib();
            auto &OLL = J.getObjLinkingLayer();

            auto PSG =
                llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
                    J.getDataLayout().getGlobalPrefix());
            if (!PSG) {
              return PSG.takeError();
            }

            MainJD.addGenerator(std::move(*PSG));

#if defined(__APPLE__)
            // Optional: warn and continue if unavailable.
            if (auto DataGen = llvm::kaldo::orc::plugin::
                    MachOHostDataSymbolGenerator::createForCurrentProcess()) {
              MainJD.addGenerator(std::move(*DataGen));
            } else {
              reportLlvmError(DataGen.takeError(),
                              "MachOHostDataSymbolGenerator");
            }
#endif

            MainJD.addGenerator(
                std::make_unique<
                    llvm::kaldo::orc::plugin::WeakSymbolFallbackGenerator>());

            auto StubsJD = J.createJITDylib(KStubsJdName);
            if (!StubsJD)
              return StubsJD.takeError();

            auto *JOL = dyn_cast<llvm::orc::ObjectLinkingLayer>(&OLL);
            if (!JOL) {
              return createStringError(inconvertibleErrorCode(),
                                       "expected a JITLink ObjectLinkingLayer");
            }

            auto RSMOrErr =
                llvm::orc::JITLinkRedirectableSymbolManager::Create(*JOL);
            if (!RSMOrErr) {
              return RSMOrErr.takeError();
            }

            auto DbgSupportErr = llvm::orc::enableDebuggerSupport(J);
            if (DbgSupportErr) {
              dbgs()
                  << "[kaldo] :: error on enabling debugger support. reason: "
                  << dumpLlvmErrorToString(std::move(DbgSupportErr)) << "\n";
            }

            JOL->addPlugin(
                std::make_unique<orc::plugin::KotlinSymbolExternalizerPlugin>(
                    *StubsJD));

#if defined(__APPLE__)
            ObjCInteropManager =
                std::make_unique<llvm::kaldo::objc::ObjCInteropManager>(J);
#endif

            SymRedirector = std::make_unique<SymbolsRedirector>(
                J, *StubsJD, std::move(*RSMOrErr));

            return Error::success();
          })
          .create();

  if (!JITOrErr) {
    return tl::make_unexpected(dumpLlvmErrorToString(JITOrErr.takeError()));
  }

  return std::unique_ptr<SymbolsOrchestrator>(
      new SymbolsOrchestrator(std::move(*JITOrErr), std::move(SymRedirector),
                              std::move(ObjCInteropManager)));
}

llvm::kaldo::SymbolsOrchestrator::~SymbolsOrchestrator() = default;
llvm::kaldo::SymbolsOrchestrator::SymbolsOrchestrator(
    llvm::kaldo::SymbolsOrchestrator &&) noexcept = default;
llvm::kaldo::SymbolsOrchestrator &llvm::kaldo::SymbolsOrchestrator::operator=(
    SymbolsOrchestrator &&) noexcept = default;

llvm::kaldo::SymbolsOrchestrator::SymbolsOrchestrator(
    std::unique_ptr<llvm::orc::LLJIT> JIT,
    std::unique_ptr<SymbolsRedirector> SymRedirector,
    std::unique_ptr<objc::ObjCInteropManager> ObjCInteroManager) {
  this->JIT = std::move(JIT);
  this->SymRedirector = std::move(SymRedirector);
  this->ObjCInteroManager = std::move(ObjCInteroManager);
}

llvm::Error llvm::kaldo::SymbolsOrchestrator::loadBootstrapPayloads(
    const manifest::Manifest &Manifest, llvm::orc::JITDylib &TargetJD,
    KotlinHotSymbols &LoadedSymbols) const {

  auto &OLL = JIT->getObjLinkingLayer();
  auto &ES = JIT->getExecutionSession();

  for (const auto &Entry : Manifest.Entries) {

    llvm::StringRef Bytes(
        reinterpret_cast<const char *>(Manifest.StartData + Entry.Offset),
        Entry.Size);

    switch (Entry.Kind) {

    case manifest::PayloadKind::kObject: {
      auto Buffer = llvm::MemoryBuffer::getMemBuffer(
          Bytes, "kaldo-bootstrap-object", false);
      LoadedSymbols +=
          KotlinHotSymbols::createFrom(*Buffer, ES.getSymbolStringPool());
      if (auto Err = JIT->addObjectFile(TargetJD, std::move(Buffer)))
        return Err;
      break;
    }
    case manifest::PayloadKind::kArchive: {
      auto Buffer = llvm::MemoryBuffer::getMemBuffer(
          Bytes, "kaldo-bootstrap-archive", false);

      // Discover the Kotlin symbols before any archive member is materialized,
      // so redirectable stubs are available when the generator is queried.
      auto VisitMembers = [&LoadedSymbols,
                           &ES](llvm::object::Archive &,
                                llvm::MemoryBufferRef MemberBuffer,
                                size_t) -> llvm::Expected<bool> {
        // Platform and system cache code is not externalized by the Kotlin
        // symbol plugin, so it must not get redirectable stubs either.
        if (MemberBuffer.getBufferIdentifier().contains("-cache.a"))
          return true;

        switch (llvm::identify_magic(MemberBuffer.getBuffer())) {
        case llvm::file_magic::elf_relocatable:
        case llvm::file_magic::macho_object:
        case llvm::file_magic::coff_object:
          LoadedSymbols += KotlinHotSymbols::createFrom(
              MemberBuffer, ES.getSymbolStringPool());
          return true;
        default:
          return false;
        }
      };

      auto GenOrErr = llvm::orc::StaticLibraryDefinitionGenerator::Create(
          OLL, std::move(Buffer), std::move(VisitMembers));
      if (!GenOrErr)
        return GenOrErr.takeError();
      TargetJD.addGenerator(std::move(*GenOrErr));
      break;
    }
    }
  }

  return llvm::Error::success();
}

tl::expected<llvm::kaldo::ReloadUnitHandle, std::string>
llvm::kaldo::SymbolsOrchestrator::loadBootstrap(
    const llvm::kaldo::manifest::Manifest &Manifest,
    const std::function<void()> &RuntimeInit,
    const std::function<void(const llvm::kaldo::objc::ObjCUniquePrefixOutput &)>
        &ObjCUniquePrefixInit,
    const std::function<void(const llvm::kaldo::objc::ObjCExportAdaptersOutput
                                 &)> &ObjCAdaptersInit) {

  auto Succeeded = false;

  auto &ES = JIT->getExecutionSession();
  auto &MainJD = JIT->getMainJITDylib();

  auto BootstrapJDOrErr = ES.createJITDylib(KBootstrapJdName);
  if (!BootstrapJDOrErr) {
    return tl::make_unexpected(
        dumpLlvmErrorToString(BootstrapJDOrErr.takeError()));
  }

  auto &BootstrapJD = *BootstrapJDOrErr;

  auto Cleanup = llvm::make_scope_exit([&] {
    if (Succeeded)
      return;
    auto RemovedErr = ES.removeJITDylib(BootstrapJD);
    if (!RemovedErr) {
      llvm::dbgs() << "[kaldo] :: Could not remove JITDylib `bootstrap`, a "
                      "leak may occur.\n";
      llvm::consumeError(std::move(RemovedErr));
    }
  });

  auto &StubsJD = SymRedirector->getStubsJD();

  BootstrapJD.addToLinkOrder(StubsJD); // StubsJD
  BootstrapJD.addToLinkOrder(MainJD);

  BootstrapJD.addGenerator(
      std::make_unique<
          llvm::kaldo::orc::plugin::WeakSymbolFallbackGenerator>());

  if (auto HostGen = orc::plugin::MachOHostDataSymbolGenerator::
          createForCurrentProcess()) {
    BootstrapJD.addGenerator(std::move(*HostGen));
  } else {
    return tl::make_unexpected(dumpLlvmErrorToString(HostGen.takeError()));
  }

  if (auto DlSymGenOrErr =
          llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
              JIT->getDataLayout().getGlobalPrefix())) {
    BootstrapJD.addGenerator(std::move(*DlSymGenOrErr));
  } else {
    return tl::make_unexpected(
        dumpLlvmErrorToString(DlSymGenOrErr.takeError()));
  }

  auto BootSymbols = llvm::kaldo::KotlinHotSymbols::create();

  if (auto LoadedCachesErr =
          loadBootstrapPayloads(Manifest, BootstrapJD, BootSymbols)) {
    return tl::make_unexpected(
        dumpLlvmErrorToString(std::move(LoadedCachesErr)));
  }

  if (auto StubsErr = SymRedirector->createRedirectableStubsIfNeeded(
          BootSymbols.getFunctions())) {
    return tl::make_unexpected(dumpLlvmErrorToString(std::move(StubsErr)));
  }

  dbgs() << "[kaldo] :: found " << BootSymbols.getFunctions().size()
         << " functions and " << BootSymbols.getClasses().size()
         << " classes in bootstrap\n";

  dbgs() << "[kaldo] :: bootstrap payloads registered!\n";

  if (auto RedirectErr = SymRedirector->redirectStubsToImplementation(
          BootstrapJD, BootSymbols.getFunctions())) {
    return tl::make_unexpected(dumpLlvmErrorToString(std::move(RedirectErr)));
  }

  if (auto ConstErr =
          callKonanConstructors(ES, JIT->getDataLayout(), BootstrapJD)) {
    return tl::make_unexpected(dumpLlvmErrorToString(std::move(ConstErr)));
  }

  RuntimeInit();

  if (ObjCInteroManager) {
    auto InitPrefixOrErr =
        ObjCInteroManager->initializeObjCUniquePrefixFromJIT(BootstrapJD);
    if (!InitPrefixOrErr) {
      return tl::make_unexpected(InitPrefixOrErr.error());
    }

    ObjCUniquePrefixInit(*InitPrefixOrErr);

    auto InitAdaptersOrErr =
        ObjCInteroManager->initializeObjCAdaptersFromJIT(BootstrapJD);
    if (!InitAdaptersOrErr) {
      return tl::make_unexpected(InitAdaptersOrErr.error());
    }

    ObjCAdaptersInit(*InitAdaptersOrErr);
  }

  auto BootstrapHandle = llvm::kaldo::ReloadUnitHandle::getBootstrapHandle();
  auto BootUnit =
      ReloadUnit(BootstrapHandle, BootstrapJD, std::move(BootSymbols));

  Succeeded = true;
  ItDidBoot = true;
  LatestReloadUnitHandle = BootstrapHandle;
  ReloadUnits.push_back(BootUnit);

  llvm::dbgs() << "[kaldo] :: bootstrap objects loaded with success!\n";

  return BootstrapHandle;
}

tl::expected<llvm::kaldo::ReloadUnitHandle, std::string>
llvm::kaldo::SymbolsOrchestrator::reloadObjects(
    const std::vector<std::string> &Paths, LoadTimings &Timings) {

  assert(!ReloadUnits.empty() &&
         "Before reloading any objects, the bootstrap loading is required.");

  auto Succeeded = false;

  auto &ES = JIT->getExecutionSession();
  auto ReloadDylibName = KReloadJdName + std::to_string(ReloadUnits.size() - 1);
  auto &StubsJD = SymRedirector->getStubsJD();
  auto &MainJD = JIT->getMainJITDylib();

  auto ReloadedJDOrErr = ES.createJITDylib(ReloadDylibName);
  if (!ReloadedJDOrErr) {
    return tl::make_unexpected(
        dumpLlvmErrorToString(ReloadedJDOrErr.takeError()));
  }

  auto &ReloadedJD = *ReloadedJDOrErr;

  auto Cleanup = llvm::make_scope_exit([&] {
    if (Succeeded)
      return;
    if (auto RemovedErr = ES.removeJITDylib(ReloadedJD)) {
      llvm::dbgs() << "[kaldo] :: Could not remove JITDylib `"
                   << ReloadDylibName << "`, a leak may occur.\n";
      llvm::consumeError(std::move(RemovedErr));
    }
  });

  for (auto It = ReloadUnits.rbegin(); It != ReloadUnits.rend(); ++It) {
    if (*It) {
      ReloadedJD.addToLinkOrder(It->value().dylib());
    }
  }

  ReloadedJD.addToLinkOrder(StubsJD);
  ReloadedJD.addToLinkOrder(MainJD);

  // Add generators directly to reloadJD so host symbols are resolvable
  // during materialization (generators on linked JITDylibs may not fire).
  if (auto HostGenOrErr = orc::plugin::MachOHostDataSymbolGenerator::
          createForCurrentProcess()) {
    ReloadedJD.addGenerator(std::move(*HostGenOrErr));
  } else {
    return tl::make_unexpected("Could not add MachOHostDataGenerator: " +
                               dumpLlvmErrorToString(HostGenOrErr.takeError()));
  }

  const auto GP = JIT->getDataLayout().getGlobalPrefix();

  if (auto DlsymGenOrErr =
          llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(GP)) {
    ReloadedJD.addGenerator(std::move(*DlsymGenOrErr));
  } else {
    return tl::make_unexpected(
        "Could not add DynamicLibrarySearchGenerator: " +
        dumpLlvmErrorToString(DlsymGenOrErr.takeError()));
  }

  auto &OLL = JIT->getObjLinkingLayer();
  auto LoadedSyms = KotlinHotSymbols::create();

  for (const auto &Path : Paths) {
    auto Buffer = readObjectFileFromPath(Path);
    if (!Buffer) {
      return tl::make_unexpected("Could not read reload input at path: " +
                                 Path);
    }

    switch (llvm::identify_magic(Buffer->getBuffer())) {
    case llvm::file_magic::elf_relocatable:
    case llvm::file_magic::macho_object:
    case llvm::file_magic::coff_object:
      LoadedSyms +=
          KotlinHotSymbols::createFrom(*Buffer, ES.getSymbolStringPool());
      if (auto AddObjErr = JIT->addObjectFile(ReloadedJD, std::move(Buffer))) {
        return tl::make_unexpected(dumpLlvmErrorToString(std::move(AddObjErr)));
      }
      break;
    case llvm::file_magic::archive: {
      auto VisitMembers = [&LoadedSyms, &ES](llvm::object::Archive &,
                                             llvm::MemoryBufferRef MemberBuffer,
                                             size_t) -> llvm::Expected<bool> {
        // Keep cache artifacts consistent with KotlinSymbolExternalizerPlugin.
        if (MemberBuffer.getBufferIdentifier().contains("-cache.a"))
          return true;

        switch (llvm::identify_magic(MemberBuffer.getBuffer())) {
        case llvm::file_magic::elf_relocatable:
        case llvm::file_magic::macho_object:
        case llvm::file_magic::coff_object:
          LoadedSyms += KotlinHotSymbols::createFrom(MemberBuffer,
                                                     ES.getSymbolStringPool());
          return true;
        default:
          return false;
        }
      };

      auto GenOrErr = llvm::orc::StaticLibraryDefinitionGenerator::Create(
          OLL, std::move(Buffer), std::move(VisitMembers));
      if (!GenOrErr) {
        return tl::make_unexpected(dumpLlvmErrorToString(GenOrErr.takeError()));
      }
      ReloadedJD.addGenerator(std::move(*GenOrErr));
      break;
    }
    default:
      return tl::make_unexpected("Unsupported reload input format at path: " +
                                 Path);
    }
  }

  if (auto StubsErr = SymRedirector->createRedirectableStubsIfNeeded(
          LoadedSyms.getFunctions())) {
    return tl::make_unexpected(dumpLlvmErrorToString(std::move(StubsErr)));
  }

  if (auto RedirectErr = SymRedirector->redirectStubsToImplementation(
          ReloadedJD, LoadedSyms.getFunctions())) {
    return tl::make_unexpected(dumpLlvmErrorToString(std::move(RedirectErr)));
  }

  auto NewId = ++CurrentReloadHandleId;
  auto Handle = ReloadUnitHandle(NewId);
  ReloadUnits.push_back(ReloadUnit(Handle, ReloadedJD, std::move(LoadedSyms)));
  LatestReloadUnitHandle = Handle;
  Succeeded = true;

  return Handle;
}

tl::expected<void *, std::string>
llvm::kaldo::SymbolsOrchestrator::lookupIn(const ReloadUnitHandle ReloadHandle,
                                           const char *SymbolName) const {

  if (!ReloadHandle)
    return nullptr;

  const uint32_t Idx = ReloadHandle.Id - 1;
  if (Idx >= ReloadUnits.size())
    return nullptr;

  auto &SlotOrOpt = ReloadUnits[Idx];
  if (!SlotOrOpt.has_value())
    return nullptr;

  auto &Slot = SlotOrOpt.value();

  auto &ES = JIT->getExecutionSession();
  auto SymOrErr = ES.lookup(llvm::orc::makeJITDylibSearchOrder(&Slot.dylib()),
                            ES.intern(SymbolName));
  if (!SymOrErr) {
    return tl::make_unexpected(dumpLlvmErrorToString(SymOrErr.takeError()));
  }

  return SymOrErr->toPtr<void *>();
}

tl::expected<void *, std::string>
llvm::kaldo::SymbolsOrchestrator::lookupInBootstrap(
    const char *SymbolName) const {

  auto &ES = JIT->getExecutionSession();
  auto *const BootstrapJD = ES.getJITDylibByName(KBootstrapJdName);
  if (!BootstrapJD) {
    return tl::make_unexpected(
        "Cannot find bootstrap library. Did you initialize the bootstrap?");
  }

  auto Mangler = llvm::orc::MangleAndInterner(ES, JIT->getDataLayout());

  auto SymOrErr = ES.lookup(llvm::orc::makeJITDylibSearchOrder(BootstrapJD),
                            Mangler(SymbolName));
  if (!SymOrErr) {
    return tl::make_unexpected(dumpLlvmErrorToString(SymOrErr.takeError()));
  }
  return SymOrErr->toPtr<void *>();
}

tl::expected<std::vector<llvm::kaldo::TypeInfoPatch>, std::string>
llvm::kaldo::SymbolsOrchestrator::collectTypeInfoPatchesFromLatestUnit() const {

  auto &ES = JIT->getExecutionSession();
  std::vector<llvm::kaldo::TypeInfoPatch> Patches;

  std::vector<llvm::orc::JITDylib *> PreviousJDs;
  for (const auto &Unit : llvm::reverse(ReloadUnits)) {
    if (!Unit.has_value())
      continue; // ignore the tombstone
    if (Unit->handle() == LatestReloadUnitHandle)
      continue; // ignore the latest handle

    PreviousJDs.push_back(&Unit->dylib());
  }

  const auto LatestReloadUnit =
      ReloadUnits[LatestReloadUnitHandle.Id - 1].value();

  // New Search Order
  const auto NSO = llvm::orc::makeJITDylibSearchOrder(
      &LatestReloadUnit.dylib(),
      llvm::orc::JITDylibLookupFlags::MatchAllSymbols);

  // Old Search Order
  const auto OSO = llvm::orc::makeJITDylibSearchOrder(PreviousJDs);

  for (auto &ClassName : LatestReloadUnit.loadedSymbols().getClasses()) {
    auto NewTypeInfoOrErr = ES.lookup(NSO, ES.intern(*ClassName));
    if (!NewTypeInfoOrErr) {
      return tl::make_unexpected(
          dumpLlvmErrorToString(NewTypeInfoOrErr.takeError()));
    }

    auto OldTypeInfoOrErr = ES.lookup(OSO, ES.intern(*ClassName));
    if (!OldTypeInfoOrErr) {
      return tl::make_unexpected(
          dumpLlvmErrorToString(OldTypeInfoOrErr.takeError()));
    }

    Patches.emplace_back(OldTypeInfoOrErr->getAddress().toPtr<void *>(),
                         NewTypeInfoOrErr->getAddress().toPtr<void *>());
  }

  return Patches;
}

tl::expected<void *, std::string>
llvm::kaldo::SymbolsOrchestrator::lookupForKonanStart() const {
  return lookupInBootstrap(KKonanStartSymbol);
}
