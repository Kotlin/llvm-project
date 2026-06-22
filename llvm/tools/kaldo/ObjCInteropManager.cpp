#include "ObjCInteropManager.hpp"
#include "Utility.hpp"

namespace {
constexpr auto KObjCInteropUniquePrefix = "_Kotlin_ObjCInterop_uniquePrefix";

constexpr auto KObjCExportSortedClassAdapters =
    "_Kotlin_ObjCExport_sortedClassAdapters";
constexpr auto KObjCExportSortedClassAdaptersNum =
    "_Kotlin_ObjCExport_sortedClassAdaptersNum";
constexpr auto KObjCExportSortedProtocolsAdapters =
    "_Kotlin_ObjCExport_sortedProtocolAdapters";
constexpr auto KObjCExportSortedProtocolsAdaptersNum =
    "_Kotlin_ObjCExport_sortedProtocolAdaptersNum";
} // namespace

template <typename T>
static tl::expected<T, std::string>
lookupSymbol(llvm::orc::ExecutionSession &ES,
             const llvm::orc::JITDylibSearchOrder &Order,
             const llvm::orc::SymbolStringPtr &Name) {
  auto Result = ES.lookup(Order, Name);
  if (!Result) {
    return tl::make_unexpected(
        llvm::kaldo::dumpLlvmErrorToString(Result.takeError()));
  }
  return *Result->getAddress().toPtr<T *>();
}

tl::expected<llvm::kaldo::objc::ObjCAdapterTable, std::string>
llvm::kaldo::objc::ObjCInteropManager::lookupObjCExportAdapters(
    llvm::orc::JITDylib &BootstrapJD, const char *AdaptersSym,
    const char *AdaptersNumSym) const {

  auto &ES = JIT.getExecutionSession();

  const ObjCTypeAdapter **Adapters{nullptr};
  int NumAdapters{0};

  const auto BootstrapSearchOrder = llvm::orc::makeJITDylibSearchOrder(
      {&BootstrapJD}, llvm::orc::JITDylibLookupFlags::MatchAllSymbols);

  const auto AdaptersName = ES.intern(AdaptersSym);
  const auto AdaptersNumName = ES.intern(AdaptersNumSym);

  const auto AdaptersResultOrErr = lookupSymbol<const ObjCTypeAdapter **>(
      ES, BootstrapSearchOrder, AdaptersName);
  if (!AdaptersResultOrErr) {
    return tl::make_unexpected(AdaptersResultOrErr.error());
  }

  const auto AdaptersNumResultOrErr =
      lookupSymbol<int>(ES, BootstrapSearchOrder, AdaptersNumName);
  if (!AdaptersNumResultOrErr) {
    return tl::make_unexpected(AdaptersNumResultOrErr.error());
  }

  if (AdaptersResultOrErr.has_value()) {
    Adapters = AdaptersResultOrErr.value();
  }

  if (AdaptersNumResultOrErr.has_value()) {
    NumAdapters = AdaptersNumResultOrErr.value();
  }

  return llvm::kaldo::objc::ObjCAdapterTable{Adapters, NumAdapters};
}

tl::expected<llvm::kaldo::objc::ObjCUniquePrefixOutput, std::string>
llvm::kaldo::objc::ObjCInteropManager::initializeObjCUniquePrefixFromJIT(
    llvm::orc::JITDylib &BootstrapJD) const {
  auto &ES = JIT.getExecutionSession();
  const auto PrefixName = ES.intern(KObjCInteropUniquePrefix);

  // Look up from BootstrapJD where bootstrap.o defines the real value.
  // MainJD's DynamicLibrarySearchGenerator would find the host's weak null
  // symbol instead. Use MatchAllSymbols because these data symbols are "private
  // external" (N_PEXT) in the object file, which maps to Scope::Hidden in
  // JITLink. The default MatchExportedSymbolsOnly would skip them.
  const auto BSO = llvm::orc::makeJITDylibSearchOrder(
      {&BootstrapJD}, llvm::orc::JITDylibLookupFlags::MatchAllSymbols);
  auto PrefixSymOrErr = ES.lookup(BSO, PrefixName);

  if (!PrefixSymOrErr) {
    return tl::make_unexpected(
        llvm::kaldo::dumpLlvmErrorToString(PrefixSymOrErr.takeError()));
  }

  const auto *const JitPrefix = *PrefixSymOrErr->getAddress().toPtr<const char **>();

  if (JitPrefix == nullptr) {
    return tl::make_unexpected(
        std::string{"UniquePrefix symbol is null in JIT'd code"});
  }

  return ObjCUniquePrefixOutput{JitPrefix};
}

tl::expected<llvm::kaldo::objc::ObjCExportAdaptersOutput, std::string>
llvm::kaldo::objc::ObjCInteropManager::initializeObjCAdaptersFromJIT(
    llvm::orc::JITDylib &BootstrapJD) const {

  const auto ClassAdaptersOrErr =
      lookupObjCExportAdapters(BootstrapJD, KObjCExportSortedClassAdapters,
                               KObjCExportSortedClassAdaptersNum);
  if (!ClassAdaptersOrErr) {
    return tl::make_unexpected(ClassAdaptersOrErr.error());
  }

  const auto [ClassAdapters, ClassAdaptersNum] = ClassAdaptersOrErr.value();

  const auto SortedProtocolAdaptersOrErr =
      lookupObjCExportAdapters(BootstrapJD, KObjCExportSortedProtocolsAdapters,
                               KObjCExportSortedProtocolsAdaptersNum);

  if (!SortedProtocolAdaptersOrErr) {
    return tl::make_unexpected(SortedProtocolAdaptersOrErr.error());
  }

  const auto [SortedProtocolAdapters, SortedProtocolAdaptersNum] =
      SortedProtocolAdaptersOrErr.value();

  if (ClassAdaptersNum == 0 && SortedProtocolAdaptersNum == 0) {
    return tl::make_unexpected(std::string{
        "Could not initialize Objective-C ClassAdapters or ProtocolAdapters"});
  }

  return ObjCExportAdaptersOutput{ClassAdapters, ClassAdaptersNum,
                                  SortedProtocolAdapters,
                                  SortedProtocolAdaptersNum};
}
