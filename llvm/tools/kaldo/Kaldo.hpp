#ifndef LLVM_KALDO_H
#define LLVM_KALDO_H

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Expected.hpp"
#include "ObjCInterop.hpp"
#include "ReloadUnitHandle.hpp"

namespace llvm {
class Error;
}

namespace llvm::orc {
class LLJIT;
class JITDylib;
} // namespace llvm::orc

namespace llvm::kaldo {

namespace objc {
class ObjCInteropManager;
}

class ReloadUnit;
struct KotlinHotSymbols;
class SymbolsRedirector;

// Public API Surface
namespace manifest {
static constexpr char FormatIdentifier[] = "KALD0";

enum class PayloadKind : uint8_t {
  kObject = 0,
  kArchive = 1,
};

struct EntryMetadata {
  PayloadKind Kind;
  uint64_t Offset;
  uint64_t Size;
};

struct Manifest {
  const uint8_t *StartData;
  uint64_t ManifestSize;
  uint64_t BundleSize;
  std::vector<EntryMetadata> Entries;
};

std::optional<Manifest>
parseKaldoManifestFromBytes(const uint8_t *Data) noexcept;

} // namespace manifest

struct LoadTimings {
  uint64_t LoadNs{0};
  uint64_t StubsNs{0};
  uint64_t RedirectNs{0};
};

/// Represents a remapping from an outdated TypeInfo to its reloaded
/// counterpart. Used during hot-reload to patch live heap objects whose class
/// implementation has changed; the runtime walks live instances and replaces
/// any pointer to `OldTypeInfo` with `NewTypeInfo`.
struct TypeInfoPatch {
  void *OldTypeInfo{nullptr};
  void *NewTypeInfo{nullptr};

  TypeInfoPatch(void *OTI, void *NTI) : OldTypeInfo(OTI), NewTypeInfo(NTI) {}
};

// Public API Surface
struct Config {
  /// Path to the On-Request-Compilation runtime library.
  std::string OrcRuntimePath;
  /// Konan Objective-C interop is enabled.
  bool KonanObjCInteropEnabled{false};
};

// Public-API Surface
class SymbolsOrchestrator {
public:
  static tl::expected<std::unique_ptr<SymbolsOrchestrator>, std::string>
  create(const Config &Config);

  ~SymbolsOrchestrator();
  SymbolsOrchestrator(SymbolsOrchestrator &&) noexcept;
  SymbolsOrchestrator &operator=(SymbolsOrchestrator &&) noexcept;

  bool itDidBoot() const { return ItDidBoot; }

  /// Load the bootstrap payloads from the manifest to start user code.
  tl::expected<ReloadUnitHandle, std::string> loadBootstrap(
      const manifest::Manifest &Manifest,
      const std::function<void()> &RuntimeInit,
      const std::function<void(const objc::ObjCUniquePrefixOutput &)>
          &ObjCUniquePrefixInit = {},
      const std::function<void(const objc::ObjCExportAdaptersOutput &)>
          &ObjCAdaptersInit = {});
  tl::expected<ReloadUnitHandle, std::string>
  reloadObjects(const std::vector<std::string> &Paths, LoadTimings &Timings);

  tl::expected<void *, std::string> lookupForKonanStart() const;

  /// Given a reload unit handle, check for the presence of the symbol with name
  /// equals to `symbolName`. If that is not the case, return a null pointer.
  tl::expected<void *, std::string> lookupIn(ReloadUnitHandle ReloadHandle,
                                             const char *SymbolName) const;

  tl::expected<void *, std::string>
  lookupInBootstrap(const char *SymbolName) const;

  tl::expected<std::vector<TypeInfoPatch>, std::string>
  collectTypeInfoPatchesFromLatestUnit() const;

private:
  uint32_t CurrentReloadHandleId{ReloadUnitHandle::BootstrapRaw};
  bool ItDidBoot{false};

  std::unique_ptr<llvm::orc::LLJIT> JIT;
  std::unique_ptr<SymbolsRedirector> SymRedirector;
  std::unique_ptr<objc::ObjCInteropManager> ObjCInteroManager;

  std::vector<std::optional<ReloadUnit>>
      ReloadUnits; // Using tombstones technique here

  ReloadUnitHandle LatestReloadUnitHandle{};

  explicit SymbolsOrchestrator(
      std::unique_ptr<llvm::orc::LLJIT> JIT,
      std::unique_ptr<SymbolsRedirector> SymRedirector,
      std::unique_ptr<llvm::kaldo::objc::ObjCInteropManager> ObjCInteroManager);

  llvm::Error loadBootstrapPayloads(const manifest::Manifest &Manifest,
                                    llvm::orc::JITDylib &TargetJD,
                                    KotlinHotSymbols &LoadedSymbols) const;
};

} // namespace llvm::kaldo

#endif // LLVM_KALDO_H
