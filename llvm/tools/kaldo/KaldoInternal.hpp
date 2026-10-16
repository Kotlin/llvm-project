#ifndef LLVM_KALDO_INTERNAL
#define LLVM_KALDO_INTERNAL

#include "Expected.hpp"
#include "ReloadUnitHandle.hpp"

#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/CoreContainers.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/RedirectionManager.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBufferRef.h"

namespace llvm::kaldo {

inline constexpr auto KKotlinFunPrefix = "_kfun:";
inline constexpr auto KKotlinClassPrefix = "_kclass:";
inline constexpr auto KImplSymbolSuffix = "$knhr";

struct KotlinHotSymbols {
private:
  /// All the Kotlin/Native functions contained in an object file (i.e.,
  /// functions starting with `kfun`).
  llvm::orc::SymbolNameSet Functions{};
  /// All the Kotlin/Native classes contained in an object file (i.e., typeinfo
  /// symbol names starting with `kclass`).
  llvm::orc::SymbolNameSet Classes{};

  KotlinHotSymbols() {}

public:

  KotlinHotSymbols& operator+=(KotlinHotSymbols &&Other) {
    Functions.insert(Other.Functions.begin(), Other.Functions.end());
    Classes.insert(Other.Classes.begin(), Other.Classes.end());
    return *this;
  }

  KotlinHotSymbols operator+(KotlinHotSymbols Other) const {
    Other += KotlinHotSymbols(*this);
    return Other;
  }

  static KotlinHotSymbols create() { return KotlinHotSymbols{}; }

  static KotlinHotSymbols
  createFrom(const llvm::MemoryBufferRef &Buf,
             const std::shared_ptr<llvm::orc::SymbolStringPool> &SSP);

  const llvm::orc::SymbolNameSet &getFunctions() const { return Functions; }

  const llvm::orc::SymbolNameSet &getClasses() const { return Classes; }
};

class ReloadUnit {
public:

  const ReloadUnitHandle& handle() const { return Handle; }

  llvm::orc::JITDylib &dylib() const { return JD; }
  const KotlinHotSymbols &loadedSymbols() const { return Symbols; }

  ReloadUnit(const ReloadUnitHandle H, llvm::orc::JITDylib &Lib, KotlinHotSymbols &&Obj)
      : Handle(H), JD{Lib}, Symbols{Obj} {}

private:
  ReloadUnitHandle Handle;

  llvm::orc::JITDylib &JD;
  // llvm::orc::ResourceTrackerSP RT;
  KotlinHotSymbols Symbols;
};

class SymbolsRedirector {

public:
  explicit SymbolsRedirector(llvm::orc::LLJIT &J, llvm::orc::JITDylib &Stubs,
                             std::unique_ptr<llvm::orc::RedirectableSymbolManager> RSM)
      : JIT{J}, StubsJD{Stubs}, RSM{std::move(RSM)} {}

  llvm::Error
  createRedirectableStubsIfNeeded(const llvm::orc::SymbolNameSet &FunctionSymbols);

  llvm::Error
  redirectStubsToImplementation(llvm::orc::JITDylib &JD,
                                const llvm::orc::SymbolNameSet &SymbolNames) const;

  llvm::orc::JITDylib &getStubsJD() const { return StubsJD; }

private:
  llvm::orc::LLJIT &JIT;
  llvm::orc::JITDylib &StubsJD;
  std::unique_ptr<llvm::orc::RedirectableSymbolManager> RSM;

  llvm::DenseSet<llvm::orc::SymbolStringPtr> RedirectableSymbols;

};

} // namespace llvm::kaldo

#endif
