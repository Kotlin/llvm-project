#ifndef LLVM_RELOADUNITHANDLE_H
#define LLVM_RELOADUNITHANDLE_H

#include <cstdint>

namespace llvm::kaldo {

// Public API Surface
struct ReloadUnitHandle {

  uint32_t Id{0};
  explicit operator bool() const { return Id != 0; }

  ReloadUnitHandle() : Id(0) {}
  explicit ReloadUnitHandle(const uint32_t HandleId) : Id(HandleId) {}

  constexpr static auto BootstrapRaw = 0x01;

  static ReloadUnitHandle getBootstrapHandle() {
    return ReloadUnitHandle(BootstrapRaw);
  }

  bool operator==(const ReloadUnitHandle &Other) const {
    return this->Id == Other.Id;
  }
};

} // namespace llvm::kaldo

#endif // LLVM_RELOADUNITHANDLE_H
