#ifndef LLVM_MACHOHOSTDATASYMBOLGENERATOR_H
#define LLVM_MACHOHOSTDATASYMBOLGENERATOR_H

#include "llvm/ExecutionEngine/Orc/Core.h"

namespace llvm::kaldo::orc::plugin {

class MachOHostDataSymbolGenerator : public llvm::orc::DefinitionGenerator {
public:
  static llvm::Expected<std::unique_ptr<MachOHostDataSymbolGenerator>>
  createForCurrentProcess();

  llvm::Error tryToGenerate(llvm::orc::LookupState &LS, llvm::orc::LookupKind K,
                            llvm::orc::JITDylib &JD,
                            llvm::orc::JITDylibLookupFlags JDLookupFlags,
                            const llvm::orc::SymbolLookupSet &Symbols) override;

private:
  explicit MachOHostDataSymbolGenerator(
      llvm::StringMap<llvm::orc::ExecutorAddr> Symbols)
      : DataSymbols(std::move(Symbols)) {}

  llvm::StringMap<llvm::orc::ExecutorAddr> DataSymbols;
};

} // namespace llvm::kaldo::orc::plugin

#endif // LLVM_MACHOHOSTDATASYMBOLGENERATOR_H
