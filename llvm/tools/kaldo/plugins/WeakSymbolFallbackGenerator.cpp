#include "WeakSymbolFallbackGenerator.hpp"

#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"

#include "../KaldoInternal.hpp"

extern "C" void weakStubFallbackFactory() {
  std::fprintf(
      stderr,
      "error :: Failed to resolve symbol from WeakSymbolFallbackGenerator\n");
  std::abort();
}

llvm::Error
llvm::kaldo::orc::plugin::WeakSymbolFallbackGenerator::tryToGenerate(
    llvm::orc::LookupState &LS, llvm::orc::LookupKind K,
    llvm::orc::JITDylib &JD, llvm::orc::JITDylibLookupFlags JDLookupFlags,
    const llvm::orc::SymbolLookupSet &Symbols) {

  // TODO: very unlikely not needed, if not for debugging purposes.

  llvm::orc::SymbolMap NewSymbols;

  for (const auto &[name, flags] : Symbols) {
    auto NameStr = (*name).str();

    // Skip kfun: symbols, they should be resolved from the loaded object files
    if (NameStr.find(llvm::kaldo::KKotlinFunPrefix) != std::string::npos)
      continue;

    // Skip C++ RTTI symbols, they are critical for exception handling and must
    // be properly exported from stdlib-cache.a
    if (NameStr.find("__ZTI") == 0 || NameStr.find("__ZTS") == 0) {
      continue;
    }

    if (NameStr.find("_NSAccessibility") == 0 ||
        NameStr.find("_mach_vm_") == 0) {
      dbgs() << "[kaldo] ::WeakSymbolFallbackGenerator: providing null definition for: "
             << NameStr << "\n";
      NewSymbols[name] = {
          llvm::orc::ExecutorAddr::fromPtr(weakStubFallbackFactory),
          llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Weak};
    }
  }

  if (!NewSymbols.empty()) {
    return JD.define(llvm::orc::absoluteSymbols(std::move(NewSymbols)));
  }

  return llvm::Error::success();
}
