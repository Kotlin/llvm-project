#include "MachOHostDataSymbolGenerator.hpp"

#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/MachO.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>

extern "C" {
/// This function symbol is defined within the Kotlin/Native runtime.
/// Its purpose is to load Kotlin-2-Objective-C bridges.

/// For this plugin, however, it is used as a landmark to determine the
/// dyld image index of the binary containing the Kotlin/Native runtime.
void *KNHR_LoadObjCStubAddress(const char *);
}

/// Find the dyld image index for the image containing the Kotlin/Native
/// runtime. Returns -1 if not found.
static int findHostImageIndex() {
  Dl_info Info;
  if (!dladdr(reinterpret_cast<void *>(&KNHR_LoadObjCStubAddress), &Info) ||
      !Info.dli_fname)
    return -1;

  const uint32_t ImageCount = _dyld_image_count();
  for (uint32_t I = 0; I < ImageCount; I++) {
    const char *Name = _dyld_get_image_name(I);
    if (Name && strcmp(Name, Info.dli_fname) == 0)
      return static_cast<int>(I);
  }
  return -1;
}

llvm::Expected<std::unique_ptr<llvm::kaldo::orc::plugin::MachOHostDataSymbolGenerator>> llvm::kaldo::orc::
    plugin::MachOHostDataSymbolGenerator::createForCurrentProcess() {

  const int ImageIndex = findHostImageIndex();

  if (ImageIndex < 0)
    return llvm::make_error<llvm::StringError>(
        "Failed to find host image in dyld image list",
        llvm::inconvertibleErrorCode());

  const char *ExecPath = _dyld_get_image_name(ImageIndex);
  if (!ExecPath)
    return llvm::make_error<llvm::StringError>("Failed to get image path",
                                               llvm::inconvertibleErrorCode());

  auto Slide = static_cast<uint64_t>(_dyld_get_image_vmaddr_slide(ImageIndex));

  auto BufOrErr = llvm::MemoryBuffer::getFile(ExecPath);
  if (!BufOrErr)
    return llvm::make_error<llvm::StringError>("Failed to read host binary",
                                               BufOrErr.getError());

  auto BinOrErr = llvm::object::createBinary((*BufOrErr)->getMemBufferRef());
  if (!BinOrErr)
    return BinOrErr.takeError();

  auto *MachO = llvm::dyn_cast<llvm::object::MachOObjectFile>(BinOrErr->get());
  if (!MachO)
    return llvm::make_error<llvm::StringError>(
        "Host binary is not a MachO file", llvm::inconvertibleErrorCode());

  llvm::StringMap<llvm::orc::ExecutorAddr> DataSymbols;

  for (const auto &Sym : MachO->symbols()) {
    auto FlagsOrErr = Sym.getFlags();
    if (!FlagsOrErr) {
      llvm::consumeError(FlagsOrErr.takeError());
      continue;
    }

    unsigned Flags = *FlagsOrErr;
    if (Flags & llvm::object::SymbolRef::SF_Undefined)
      continue;
    if (Flags & llvm::object::SymbolRef::SF_FormatSpecific)
      continue;

    auto NameOrErr = Sym.getName();
    if (!NameOrErr) {
      llvm::consumeError(NameOrErr.takeError());
      continue;
    }

    auto AddrOrErr = Sym.getAddress();
    if (!AddrOrErr) {
      llvm::consumeError(AddrOrErr.takeError());
      continue;
    }

    DataSymbols[*NameOrErr] = llvm::orc::ExecutorAddr(*AddrOrErr + Slide);
  }

  dbgs() << "[kaldo] :: MachOHostDataSymbolGenerator indexed " << DataSymbols.size() << " symbols\n";

  return std::unique_ptr<MachOHostDataSymbolGenerator>(
      new MachOHostDataSymbolGenerator(std::move(DataSymbols)));
}

llvm::Error llvm::kaldo::orc::plugin::MachOHostDataSymbolGenerator::tryToGenerate(
    llvm::orc::LookupState &LS, llvm::orc::LookupKind K,
    llvm::orc::JITDylib &JD, llvm::orc::JITDylibLookupFlags JDLookupFlags,
    const llvm::orc::SymbolLookupSet &Symbols) {
  llvm::orc::SymbolMap NewSymbols;

  for (const auto &[name, flags] : Symbols) {
    auto It = DataSymbols.find(*name);
    if (It != DataSymbols.end())
      NewSymbols[name] = {It->second, llvm::JITSymbolFlags::Exported};
  }

  if (!NewSymbols.empty())
    return JD.define(llvm::orc::absoluteSymbols(std::move(NewSymbols)));

  return llvm::Error::success();
}
