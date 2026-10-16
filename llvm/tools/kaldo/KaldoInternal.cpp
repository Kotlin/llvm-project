#include "KaldoInternal.hpp"
#include "Kaldo.hpp"

#include "llvm/Support/Error.h"

llvm::kaldo::KotlinHotSymbols
llvm::kaldo::KotlinHotSymbols::createFrom(const llvm::MemoryBufferRef &Buf,
    const std::shared_ptr<llvm::orc::SymbolStringPool> &SSP) {

  KotlinHotSymbols Result{};
  auto GraphOrErr = llvm::jitlink::createLinkGraphFromObject(Buf, SSP);

  if (!GraphOrErr) {
    return Result;
  }

  const auto &LinkGraph = *GraphOrErr;
  for (auto &Section : LinkGraph->sections()) {
    for (const auto *Symbol : Section.symbols()) {
      if (!Symbol->hasName())
        continue;
      if (Symbol->getScope() == llvm::jitlink::Scope::Local)
         continue;

      const auto SymbolName = *Symbol->getName();

      if (SymbolName.starts_with(KKotlinFunPrefix)) {
        Result.Functions.insert(SSP->intern(SymbolName));
      } else if (SymbolName.starts_with(KKotlinClassPrefix)) {
        Result.Classes.insert(SSP->intern(SymbolName));
      }
    }
  }

  return Result;
}

llvm::Error llvm::kaldo::SymbolsRedirector::createRedirectableStubsIfNeeded(
    const llvm::orc::SymbolNameSet &FunctionSymbols) {

  llvm::orc::SymbolMap InitialDestinations{};

  auto &ES = JIT.getExecutionSession();

  for (auto &SymbolName : FunctionSymbols) {
    auto Interned = ES.intern(*SymbolName);
    if (RedirectableSymbols.contains(Interned))
      continue;

    // Let's create a new stub symbol with the same name as the original one
    InitialDestinations[Interned] = llvm::orc::ExecutorSymbolDef(
        llvm::orc::ExecutorAddr(),
        llvm::JITSymbolFlags::Callable | llvm::JITSymbolFlags::Exported);
    RedirectableSymbols.insert(Interned);
  }

  if (InitialDestinations.empty())
    return llvm::Error::success();

  auto Err = RSM->createRedirectableSymbols(StubsJD.getDefaultResourceTracker(),
                                       std::move(InitialDestinations));

  return Err;
}

llvm::Error llvm::kaldo::SymbolsRedirector::redirectStubsToImplementation(
    llvm::orc::JITDylib &JD, const llvm::orc::SymbolNameSet &SymbolNames) const {

  auto &ES = JIT.getExecutionSession();

  // Force materialization of the impl MU by looking up original names.
  // This triggers the KotlinSymbolExternalizer plugin, which creates $knhr impl
  // symbols via MR.defineMaterializing() and replaces originals with reexports
  // from StubsJD (also materializing the stubs, creating $__stub_ptr symbols).
  {
    llvm::orc::SymbolLookupSet TriggerSymbols{};
    for (auto &SymName : SymbolNames) {
      TriggerSymbols.add(ES.intern(*SymName));
    }

    auto TriggerResult =
        ES.lookup(llvm::orc::makeJITDylibSearchOrder(
                      {&JD}, llvm::orc::JITDylibLookupFlags::MatchAllSymbols),
                  std::move(TriggerSymbols));

    if (!TriggerResult) {
      return TriggerResult.takeError();
    }
  }

  // Now look up $knhr impl addresses (created by the plugin during previous
  // materialization)
  llvm::orc::SymbolMap Dests{};

  for (auto &SymName : SymbolNames) {
    std::string ImplName = (*SymName + KImplSymbolSuffix).str();
    auto SymOrErr =
        ES.lookup(llvm::orc::makeJITDylibSearchOrder(
                      {&JD}, llvm::orc::JITDylibLookupFlags::MatchAllSymbols),
                  ES.intern(ImplName));
    if (!SymOrErr) {
      dbgs() << "[kaldo] :: failed to lookup implementation symbol: "
             << ImplName << "\n";
      llvm::consumeError(SymOrErr.takeError());
      continue;
    }
    Dests[ES.intern(*SymName)] = SymOrErr.get();
  }

  if (Dests.empty())
    return llvm::Error::success();

  return RSM->redirect(StubsJD, Dests);
}
