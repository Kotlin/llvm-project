#ifndef LLVM_UTILITY_H
#define LLVM_UTILITY_H

#include "llvm/Support/Error.h"

namespace llvm::kaldo {

inline void reportLlvmError(llvm::Error E, const llvm::Twine &Context) {
  logAllUnhandledErrors(std::move(E), llvm::errs(),
                        (llvm::Twine("[kaldo] ") + Context + ": ").str());
}

inline std::string dumpLlvmErrorToString(llvm::Error E) {
  std::string ErrorMessage{};

  llvm::handleAllErrors(std::move(E), [&](const llvm::ErrorInfoBase &EIB) {
    ErrorMessage += EIB.message() + "\n";
  });

  if (!ErrorMessage.empty() && ErrorMessage.back() == '\n') {
    ErrorMessage.pop_back();
  }

  return ErrorMessage;
}

} // namespace llvm::kaldo

#endif // LLVM_UTILITY_H
