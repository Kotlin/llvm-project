#ifndef LLVM_WEAKSYMBOLFALLBACKGENERATOR_H
#define LLVM_WEAKSYMBOLFALLBACKGENERATOR_H

#include "llvm/ExecutionEngine/Orc/Core.h"

namespace llvm::kaldo::orc::plugin {

/// Fallback definition generator that provides null/weak definitions for missing symbols.
class WeakSymbolFallbackGenerator : public llvm::orc::DefinitionGenerator {
public:
  WeakSymbolFallbackGenerator() = default;

  llvm::Error tryToGenerate(
          llvm::orc::LookupState& LS,
          llvm::orc::LookupKind K,
          llvm::orc::JITDylib& JD,
          llvm::orc::JITDylibLookupFlags JDLookupFlags,
          const llvm::orc::SymbolLookupSet& Symbols) override;
};

} // namespace llvm::kaldo::orc::plugin

#endif // LLVM_WEAKSYMBOLFALLBACKGENERATOR_H
