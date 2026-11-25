//===- DeltaMainStackMap.h - Delta-encoded GC stack map format -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data structures for the "delta-main" GC stack map format: a compact
// encoding that, instead of repeating the full set of live locations at
// every call site, stores one full base state per function plus, for every
// call site, only the symmetric difference (register/slot bitmask XOR,
// symmetric difference of derived-pointer links) against that base state.
//
// This format is emitted by KotlinNativeGCPrinter (registered for the
// "kotlin-native" GC strategy, see llvm/lib/IR/BuiltinGCs.cpp) via
// DeltaMainStackMapEncoder, and consumed by the Kotlin/Native runtime GC
// (kotlin-native/runtime/src/gc/common/cpp/stackmap/DeltaMainStackMap.*).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_DELTAMAINSTACKMAP_H
#define LLVM_CODEGEN_DELTAMAINSTACKMAP_H

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/UniqueVector.h"
#include "llvm/CodeGen/StackMaps.h"
#include <cstdint>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace llvm {

class MCExpr;
class MCStreamer;
class MCSymbol;

namespace deltamain {

using Location = StackMaps::Location;

/// The set of live locations at a single call site (one safepoint), before
/// it has been compressed against its function's base state.
struct State {
  /// Expression for this call site's offset from the function's start.
  const MCExpr *Pc = nullptr;

  /// Live stack slots holding GC roots.
  std::vector<Location> StackSlots;
};

/// All States recorded for one function, before delta-compression.
struct FunctionState {
  const MCSymbol *Symbol = nullptr;
  const MCSymbol *End = nullptr;
  int64_t BaseOffset = 0;
  uint64_t StackSize = 0;
  std::vector<State> States;

  /// Assigns a stable, 8-byte-aligned frame-relative index (written into
  /// each Location's Reg field) to every stack-slot Location referenced by
  /// States, and sets BaseOffset to the highest stack offset seen. Must run
  /// before the FunctionState is converted to Delta form, since Delta
  /// encodes slots by this index, not by raw offset.
  void assignSlotIndices(int64_t FPtoSPDelta);
};

/// NOTE: this port intentionally does not track callee-saved-register
/// liveness or derived-pointer/relocation links (see State/Delta below).
/// Register-typed GC roots are provably unreachable in this pipeline, and
/// derived-pointer tracking only matters for relocating pointers after a
/// moving/compacting GC, which Kotlin/Native does not have. Reintroducing
/// derived-pointer tracking would be required if a compacting GC were ever
/// added later.
///
/// The live-location set of one call site, encoded relative to its
/// function's base state. Bit positions are produced by
/// DeltaMainStackMapEncoder::enumerate().
struct Delta {
  BitVector StackSlots;

  Delta() = default;

  friend bool operator==(const Delta &LHS, const Delta &RHS) {
    return std::tie(LHS.StackSlots) == std::tie(RHS.StackSlots);
  }

  friend bool operator<(const Delta &LHS, const Delta &RHS) {
    return std::tie(LHS.StackSlots) < std::tie(RHS.StackSlots);
  }

  /// Serialize this Delta as a ULEB128-encoded stack-slot bit vector.
  void emit(MCStreamer &OS) const;
};

/// One function's complete delta-main record: its base state plus the
/// deduplicated set of Deltas referenced by its call sites.
struct FuncDesc {
  const MCSymbol *Symbol = nullptr;
  const MCSymbol *End = nullptr;
  int64_t BaseOffset = 0;
  uint64_t StackSize = 0;

  /// The function's first recorded State, encoded as a Delta against
  /// itself (i.e. the full live-location set at that call site).
  Delta Base;

  /// Deduplicated deltas referenced by PcToDelta. Indices into this vector
  /// are 1-based (UniqueVector convention); PcToDelta stores those indices
  /// directly.
  UniqueVector<Delta> Deltas;

  /// (call-site PC offset, 1-based index into Deltas) for every recorded
  /// call site, in recording order.
  std::vector<std::pair<const MCExpr *, unsigned>> PcToDelta;

  FuncDesc() = default;
};

/// The full delta-main stack map: one FuncDesc per function that has at
/// least one GC-relevant call site.
struct EncodedStackMap {
  std::vector<FuncDesc> Funcs;
};

} // end namespace deltamain
} // end namespace llvm

#endif // LLVM_CODEGEN_DELTAMAINSTACKMAP_H
