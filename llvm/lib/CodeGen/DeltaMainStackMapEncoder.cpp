//===- DeltaMainStackMapEncoder.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "DeltaMainStackMapEncoder.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::deltamain;

#define DEBUG_TYPE "delta-main-stackmaps"

using Location = StackMaps::Location;

//===----------------------------------------------------------------------===//
// Recovering base/derived-pointer structure from a flattened CallsiteInfo.
//===----------------------------------------------------------------------===//

std::set<deltamain::Location> DeltaMainStackMapEncoder::collectBases(
    const StackMaps::CallsiteInfo &CSI) const {
  std::set<Location> Bases;
  const StackMaps::LocationVec &Locs = CSI.Locations;

  // StackMaps::parseStatepointOpers appends, in order: 3 meta locations
  // (calling convention, flags, deopt-arg count), that many deopt-arg
  // locations, then base/derived GC-pointer pairs, then CSI.NumAllocas
  // alloca locations.
  constexpr size_t NumLeadingConstants = 3;
  constexpr size_t NumDeoptArgsIdx = 2;
  if (Locs.size() < NumLeadingConstants)
    report_fatal_error(
        "delta-main: callsite has fewer than the 3 leading statepoint meta "
        "locations (calling convention, flags, deopt-arg count)");

  assert(Locs[NumDeoptArgsIdx].Type == Location::Constant &&
        "third statepoint meta location should be the deopt-arg count");
  size_t NumDeoptArgs = Locs[NumDeoptArgsIdx].Offset;

  if (Locs.size() < NumLeadingConstants + NumDeoptArgs + CSI.NumAllocas)
    report_fatal_error("delta-main: callsite location count is inconsistent "
                       "with its recorded deopt-arg/alloca counts");

  size_t RefPairsBegin = NumLeadingConstants + NumDeoptArgs;
  size_t RefPairsEnd = Locs.size() - CSI.NumAllocas;
  size_t RefPairCount = RefPairsEnd - RefPairsBegin;
  LLVM_DEBUG(dbgs() << "NumAllocas =" << CSI.NumAllocas << "\n");
  if (RefPairCount % 2 != 0)
    report_fatal_error(
        "delta-main: base/derived GC pointers must come in pairs");

  auto IsTrackable = [](const Location &Loc) {
    return Loc.Type == Location::Indirect || Loc.Type == Location::Register;
  };

  for (size_t I = RefPairsBegin; I < RefPairsEnd; I += 2) {
    const Location &Base = Locs[I];
    const Location &Derived = Locs[I + 1];

    LLVM_DEBUG({
      auto DwarfRegName = [](uint16_t Reg) -> std::string {
        switch (Reg) {
          case 29: return "x29/fp";
          case 30: return "x30/lr";
          case 31: return "sp";
          default: return ("x" + Twine(Reg)).str();
        }
      };
      auto PrintLoc = [&](const char *Label, const Location &Loc) {
        dbgs() << "    " << Label << ": ";
        switch (Loc.Type) {
          case Location::Register:
            dbgs() << "REG  " << DwarfRegName(Loc.Reg);
            break;
          case Location::Direct:
            dbgs() << "DIR  [" << DwarfRegName(Loc.Reg) << " + " << Loc.Offset << "]";
            break;
          case Location::Indirect:
            dbgs() << "IND  [" << DwarfRegName(Loc.Reg) << " + " << Loc.Offset << "]";
            break;
          default:
            dbgs() << "UNKNOWN";
        }
        dbgs() << "  (size=" << Loc.Size << ")\n";
      };
      dbgs() << "[delta-main] pair #" << (I - RefPairsBegin) / 2 << (Base == Derived ? "  (base-only)" : "") << "\n";
      PrintLoc("base   ", Base);
      if (Base != Derived)
        PrintLoc("derived", Derived);
    });

    if (!IsTrackable(Base) || !IsTrackable(Derived))
      report_fatal_error(
          "delta-main: unsupported location kind in a base/derived GC "
          "pointer pair (expected a callee-saved register or an indirect "
          "stack slot)");

    Bases.insert(Base);
  }

  // Alloca roots are always their own base, with no separate derived
  // pointer, and (unlike the pairs above) are not restricted to
  // Indirect/Register -- gc.statepoint alloca operands are typically
  // Location::Direct (a frame address, not a loaded value).
  for (size_t I = RefPairsEnd; I < Locs.size(); ++I)
    Bases.insert(Locs[I]);

  return Bases;
}

void DeltaMainStackMapEncoder::populateState(
    State &St, const std::set<Location> &Bases) const {
  for (const Location &Loc : Bases) {
    switch (Loc.Type) {
    case Location::Direct:
      report_fatal_error(
          "delta-main: found a direct-typed GC root, but "
          "directs-in-stackmap are not supported");
    case Location::Indirect:
      St.StackSlots.push_back(Loc);
      break;

    case Location::Register:
      report_fatal_error(
          "delta-main: found a register-typed GC root, but "
          "registers-in-stackmap are not supported");

    default:
      llvm_unreachable(
          "collectBases only inserts Direct/Indirect/Register keys");
    }
  }
}

//===----------------------------------------------------------------------===//
// Per-function state collection and stack-slot index assignment.
//===----------------------------------------------------------------------===//

FunctionState DeltaMainStackMapEncoder::buildFunctionState(
    const MCSymbol *Symbol, const StackMaps::FunctionInfo &FnInfo,
    const StackMaps::CallsiteInfoList &CSInfos, unsigned StartIdx) const {
  FunctionState Func;
  Func.Symbol = Symbol;
  Func.End = FnInfo.End;
  Func.StackSize = FnInfo.StackSize;

  if (!Func.End)
    report_fatal_error("delta-main: function end label was not recorded");

  for (unsigned I = 0; I < FnInfo.RecordCount; ++I) {
    const StackMaps::CallsiteInfo &CSI = CSInfos[StartIdx + I];

    State NewState;
    NewState.Pc = CSI.CSOffsetExpr;

    std::set<Location> Bases = collectBases(CSI);
    populateState(NewState, Bases);

    Func.States.push_back(std::move(NewState));
  }

  return Func;
}

void FunctionState::assignSlotIndices(int64_t FPtoSPDelta) {
  int64_t MaxOffset = INT64_MIN;
  auto NoteOffset = [&MaxOffset](const Location &Loc) {
    MaxOffset = std::max<int64_t>(Loc.Offset, MaxOffset);
  };

  auto LocationConverter = [FPtoSPDelta](Location& Loc) {
    if (Loc.Type != StackMaps::Location::Indirect) {
      return;
    }

    if (Loc.Reg != /*Fp*/ 29) {
      if (Loc.Reg != /*Sp*/ 31) {
        report_fatal_error(
            "delta-main: stack slots must be addressed from SP or FP");
      }
      Loc.Reg = /*Fp*/ 29;
      Loc.Offset = Loc.Offset + FPtoSPDelta;
    }
  };

  for (State &St : States) {
    for (Location &Slot : St.StackSlots) {
      LocationConverter(Slot);
      NoteOffset(Slot);
    }
  }

  if (MaxOffset == INT64_MIN) {
    BaseOffset = 0; // No stack slots at all: nothing to index.
    return;
  }
  BaseOffset = MaxOffset;

  auto AssignIndex = [MaxOffset](Location &Loc) {
    int64_t SlotDelta = MaxOffset - Loc.Offset;
    if (SlotDelta % 8 != 0)
      report_fatal_error("delta-main: stack slot offset is not 8-byte "
                         "aligned relative to the function's highest live "
                         "offset");
    if (SlotDelta < 0)
      report_fatal_error("delta-main: stack slot offset exceeds the "
                         "function's highest live offset");

    Loc.Reg = SlotDelta / 8;
    Loc.Offset = MaxOffset;
  };

  for (State &St : States) {
    for (Location &Slot : St.StackSlots)
      AssignIndex(Slot);
  }
}

//===----------------------------------------------------------------------===//
// Enumeration and delta computation.
//===----------------------------------------------------------------------===//

int64_t DeltaMainStackMapEncoder::enumerate(const Location &Loc) const {
  switch (Loc.Type) {
  case Location::Direct:
  case Location::Indirect:
    return Loc.Reg; // Set by FunctionState::assignSlotIndices.

  case Location::Register:
    llvm_unreachable("enumerate: register-typed GC roots are not supported "
                     "by this port (see "
                     "DeltaMainStackMapEncoder::populateState)");

  default:
    llvm_unreachable("enumerate: unsupported Location type");
  }
}

BitVector
DeltaMainStackMapEncoder::getLocationMask(ArrayRef<Location> Locs) const {
  BitVector Vector;
  if (Locs.empty())
    return Vector;

  int64_t MaxIdx = 0;
  for (const Location &Loc : Locs)
    MaxIdx = std::max(MaxIdx, enumerate(Loc));
  Vector.resize(MaxIdx + 1);

  for (const Location &Loc : Locs)
    Vector.set(enumerate(Loc));

  return Vector;
}

Delta DeltaMainStackMapEncoder::computeDelta(const State &St) const {
  Delta D;
  D.StackSlots = getLocationMask(St.StackSlots);
  return D;
}

Delta DeltaMainStackMapEncoder::computeDelta(const State &Base,
                                             const State &Other) const {
  Delta BaseDelta = computeDelta(Base);
  Delta OtherDelta = computeDelta(Other);

  Delta D;
  D.StackSlots = BaseDelta.StackSlots;
  D.StackSlots ^= OtherDelta.StackSlots;

  return D;
}

//===----------------------------------------------------------------------===//
// Top-level build/emit.
//===----------------------------------------------------------------------===//

EncodedStackMap DeltaMainStackMapEncoder::build(StackMaps &SM) const {
  EncodedStackMap Map;

  unsigned StartIdx = 0;
  for (auto &FR : SM.getFnInfos()) {
    const MCSymbol *Symbol = FR.first;
    const StackMaps::FunctionInfo &FnInfo = FR.second;

    FunctionState Func =
        buildFunctionState(Symbol, FnInfo, SM.getCSInfos(), StartIdx);
    StartIdx += FnInfo.RecordCount;

    if (Func.States.empty())
      continue; // No GC-relevant call site in this function.

    Func.assignSlotIndices(FnInfo.FPtoSPDelta);
    Map.Funcs.push_back(buildFuncDesc(Func));
  }

  return Map;
}

FuncDesc DeltaMainStackMapEncoder::buildFuncDesc(const FunctionState &Func) const {
  assert(!Func.States.empty() &&
        "buildFuncDesc requires at least one recorded call site");

  FuncDesc Desc;
  Desc.Symbol = Func.Symbol;
  Desc.End = Func.End;
  Desc.BaseOffset = Func.BaseOffset;
  Desc.StackSize = Func.StackSize;

  const State &BaseState = Func.States.front();
  Desc.Base = computeDelta(BaseState);

  for (const State &St : Func.States) {
    Delta D = computeDelta(BaseState, St);
    unsigned Idx = Desc.Deltas.insert(D);
    Desc.PcToDelta.emplace_back(St.Pc, Idx - 1); // UniqueVector is 1-based.
  }

  return Desc;
}

void DeltaMainStackMapEncoder::emit(MCStreamer &OS,
                                    const EncodedStackMap &Map) const {
  MCContext &Ctx = OS.getContext();

  for (const FuncDesc &Func : Map.Funcs) {
    MCSymbol *FuncStackMapStart = Ctx.getOrCreateSymbol(
        ".Lstackmap_delta_main_start." + Func.Symbol->getName());
    OS.emitLabel(FuncStackMapStart);

    const MCExpr *FuncRef = MCSymbolRefExpr::create(Func.Symbol, Ctx);
    OS.emitValue(MCBinaryExpr::createSub(
                     FuncRef, MCSymbolRefExpr::create(FuncStackMapStart, Ctx),
                     Ctx),
                 4);
    // This field can be safely removed if the maps need to be compressed.
    OS.emitValue(MCBinaryExpr::createSub(
                     MCSymbolRefExpr::create(Func.End, Ctx), FuncRef, Ctx),
                 4);
    OS.emitSLEB128IntValue(Func.BaseOffset);
    OS.emitULEB128IntValue(Func.StackSize);

    Func.Base.emit(OS);

    OS.emitULEB128IntValue(Func.PcToDelta.size());
    for (const auto &PcDelta : Func.PcToDelta) {
      OS.emitULEB128Value(PcDelta.first);
      OS.emitULEB128IntValue(PcDelta.second);
    }

    for (const Delta &D : Func.Deltas)
      D.emit(OS);
  }
}
