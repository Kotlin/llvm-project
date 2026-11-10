//===- DeltaMainStackMapEncoder.h - build/emit delta-main maps -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Converts a StackMaps object's recorded per-callsite location data into the
// delta-main::EncodedStackMap representation (see
// llvm/CodeGen/DeltaMainStackMap.h) and serializes it. Used by
// KotlinNativeGCPrinter, which is the only intended caller: the encoder
// itself does not touch AsmPrinter, the "kotlin-native" GCStrategy, or the
// StackMaps section-emission machinery.
//
// NOTE: this is an implementation-detail header (lives in lib/, not
// include/) analogous to other CodeGen-internal helper headers such as
// llvm/lib/CodeGen/SafeStackLayout.h.
//
// Current limitations of this initial port (see class comment below):
//  - AArch64 only.
//  - Register-liveness tracking is not supported at all: StackMaps does not
//    currently record which callee-saved registers are live at a callsite
//    the way it records stack slots. This will report_fatal_error as soon
//    as a register-typed GC root is encountered.
//  - Derived-pointer/relocation tracking is not implemented (this port only
//    ever targets non-moving/mark-sweep GC variants; see the rationale
//    comment in DeltaMainStackMap.h).
//  - No lazy/offset-section support: every function's delta-main record is
//    emitted unconditionally into the stackmap section, mirroring how the
//    default StackMaps::serializeToStackMapSection() behaves today.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_CODEGEN_DELTAMAINSTACKMAPENCODER_H
#define LLVM_LIB_CODEGEN_DELTAMAINSTACKMAPENCODER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/CodeGen/DeltaMainStackMap.h"
#include "llvm/CodeGen/StackMaps.h"
#include <set>

namespace llvm {

class MCStreamer;

/// Builds a deltamain::EncodedStackMap from a StackMaps object and
/// serializes it to the delta-main wire format.
///
/// This class is AArch64-only: FunctionState::assignSlotIndices hardcodes
/// AArch64 DWARF register numbers for the frame/stack pointer (see its
/// LocationConverter). Callers must verify the target triple before use;
/// the encoder itself does not check it, since doing so requires a
/// Triple/TargetMachine that KotlinNativeGCPrinter already has available
/// via AsmPrinter.
class DeltaMainStackMapEncoder {
public:
  /// Build the delta-main-encoded form of every function in \p SM that has
  /// at least one recorded, GC-relevant call site.
  deltamain::EncodedStackMap build(StackMaps &SM) const;

  /// Serialize \p Map to \p OS. Wire format, per function:
  ///   i32   function entry point minus the address of this record
  ///   i32   function size in bytes
  ///   sleb  Func.BaseOffset
  ///   uleb  Func.StackSize
  ///   ...   Func.Base (see Delta::emit)
  ///   uleb  Func.PcToDelta.size()
  ///   ...   for each entry: uleb-encoded PC offset, uleb-encoded delta index
  ///   ...   each unique Delta in Func.Deltas, in insertion order (see
  ///         Delta::emit)
  void emit(MCStreamer &OS, const deltamain::EncodedStackMap &Map) const;

private:
  /// Recover the set of unique base GC-pointer locations, and the alloca
  /// roots, that StackMaps::parseStatepointOpers flattened into
  /// \p CSI.Locations. Derived pointers are discarded: this port only
  /// tracks base pointers (see DeltaMainStackMap.h).
  std::set<deltamain::Location>
  collectBases(const StackMaps::CallsiteInfo &CSI) const;

  /// Classify every location in \p Bases into \p State's StackSlots.
  void populateState(deltamain::State &State,
                     const std::set<deltamain::Location> &Bases) const;

  /// Build the (not yet delta-compressed) per-callsite State list for one
  /// function. \p StartIdx is the index of this function's first record in
  /// \p CSInfos (StackMaps records callsites for all functions in one flat
  /// list, in the order functions were first seen).
  deltamain::FunctionState
  buildFunctionState(const MCSymbol *Symbol,
                     const StackMaps::FunctionInfo &FnInfo,
                     const StackMaps::CallsiteInfoList &CSInfos,
                     unsigned StartIdx) const;

  /// Delta-compress \p Func's States against its first recorded State and
  /// deduplicate the results.
  deltamain::FuncDesc buildFuncDesc(const deltamain::FunctionState &Func) const;

  /// Encode a single State as a Delta against an implicit all-zero state
  /// (i.e. its own full live-location set).
  deltamain::Delta computeDelta(const deltamain::State &State) const;

  /// Encode the symmetric difference between two States as a Delta.
  deltamain::Delta computeDelta(const deltamain::State &Base,
                                const deltamain::State &Other) const;

  /// Build a bit vector with bit enumerate(Loc) set for every Loc in
  /// \p Locs.
  BitVector getLocationMask(ArrayRef<deltamain::Location> Locs) const;

  /// Maps a Direct/Indirect stack-slot Location to a dense, non-negative
  /// bit-vector index (its FunctionState::assignSlotIndices-assigned slot
  /// index). Register-typed Locations are not supported: see
  /// DeltaMainStackMapEncoder::populateState.
  int64_t enumerate(const deltamain::Location &Loc) const;
};

} // end namespace llvm

#endif // LLVM_LIB_CODEGEN_DELTAMAINSTACKMAPENCODER_H
