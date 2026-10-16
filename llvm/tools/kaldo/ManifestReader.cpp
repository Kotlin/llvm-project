#include "Kaldo.hpp"

#include <cstring>

using namespace llvm::kaldo::manifest;

static constexpr uint64_t FormatIdentifierSize = sizeof(FormatIdentifier) - 1;
static constexpr uint64_t HeaderSize = FormatIdentifierSize + sizeof(uint64_t) +
                                       sizeof(uint64_t) + sizeof(uint32_t);
static constexpr uint64_t EntrySize =
    sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint64_t);

template <typename T> static T read(const uint8_t *&Ptr) {
  T Value;
  std::memcpy(&Value, Ptr, sizeof(Value));
  Ptr += sizeof(Value);
  return Value;
}

std::optional<Manifest> llvm::kaldo::manifest::parseKaldoManifestFromBytes(
    const uint8_t *Data) noexcept {

  if (!Data || std::memcmp(Data, FormatIdentifier, FormatIdentifierSize) != 0) {
    return std::nullopt;
  }
  Data += FormatIdentifierSize;

  const uint64_t ManifestSize = read<uint64_t>(Data);
  const uint64_t BundleSize = read<uint64_t>(Data);
  const uint32_t EntryCount = read<uint32_t>(Data);

  const uint64_t MetadataSize = HeaderSize + EntryCount * EntrySize;
  if (ManifestSize < MetadataSize ||
      ManifestSize - MetadataSize != BundleSize) {
    return std::nullopt;
  }

  Manifest Manifest{Data + EntryCount * EntrySize, ManifestSize, BundleSize,
                    {}};
  Manifest.Entries.reserve(EntryCount);

  for (uint32_t I = 0; I < EntryCount; ++I) {
    const auto Kind = static_cast<PayloadKind>(*Data++);
    const uint64_t Offset = read<uint64_t>(Data);
    const uint64_t Size = read<uint64_t>(Data);

    if ((Kind != PayloadKind::kObject && Kind != PayloadKind::kArchive) ||
        Offset > BundleSize || Size > BundleSize - Offset) {
      return std::nullopt;
    }
    Manifest.Entries.push_back({Kind, Offset, Size});
  }

  return Manifest;
}
