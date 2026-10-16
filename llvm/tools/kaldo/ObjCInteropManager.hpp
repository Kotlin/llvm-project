#ifndef LLVM_OBJCINTEROPMANAGER_H
#define LLVM_OBJCINTEROPMANAGER_H

#include "Expected.hpp"
#include "ObjCInterop.hpp"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"

namespace llvm::kaldo::objc {

class ObjCInteropManager {

public:
  explicit ObjCInteropManager(llvm::orc::LLJIT &J) : JIT(J) {}

  /// Initialize ObjC unique prefix from JIT'd code.
  /// This must happen before any ObjC class creation (CreateKotlinObjCClass).
  /// The compiler generates Kotlin_ObjCInterop_uniquePrefix in bootstrap.o,
  /// but the runtime has a weak symbol initialized to nullptr.
  tl::expected<ObjCUniquePrefixOutput, std::string>
  initializeObjCUniquePrefixFromJIT(llvm::orc::JITDylib &BootstrapJD) const;

  /// Initialize ObjC type adapters from symbols defined in JIT'd code.
  /// This looks up the adapter arrays from the JIT and passes them to the runtime.
  tl::expected<ObjCExportAdaptersOutput, std::string>
  initializeObjCAdaptersFromJIT(llvm::orc::JITDylib &BootstrapJD) const;

private:
  llvm::orc::LLJIT &JIT;

  tl::expected<ObjCAdapterTable, std::string>
  lookupObjCExportAdapters(orc::JITDylib &BootstrapJD, const char *AdaptersSym,
                           const char *AdaptersNumSym) const;
};

} // namespace llvm::kaldo::objc

#endif // LLVM_OBJCINTEROPMANAGER_H
