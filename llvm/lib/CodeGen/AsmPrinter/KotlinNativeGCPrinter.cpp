//===- KotlinNativeGCPrinter.cpp - Delta-main stack-map emitter ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the GCMetadataPrinter for the "kotlin-native" GC
// strategy (see llvm/lib/IR/BuiltinGCs.cpp): it emits the delta-main
// compressed stack-map format consumed by the Kotlin/Native runtime GC,
// instead of LLVM's default StackMaps format.
//
//===----------------------------------------------------------------------===//

#include "../DeltaMainStackMapEncoder.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/GCMetadataPrinter.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/IR/BuiltinGCs.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

// AArch64FrameLowering reads it to store the record address in the frame.
namespace llvm {
cl::opt<bool> KNLazyStackMap("kn-lazy-stackmap", cl::init(true), cl::Hidden);
} // namespace llvm

namespace {

class KotlinNativeGCPrinter : public GCMetadataPrinter {
public:
  bool emitStackMaps(StackMaps &SM, AsmPrinter &AP) override;
};

} // end anonymous namespace

static GCMetadataPrinterRegistry::Add<KotlinNativeGCPrinter>
    X("kotlin-native", "Kotlin/Native GC strategy (delta-main stack maps)");

void llvm::linkKotlinNativeGCPrinter() {}

bool KotlinNativeGCPrinter::emitStackMaps(StackMaps &SM, AsmPrinter &AP) {
  // This port's encoder relies on an AArch64-specific register-enumeration
  // scheme (see DeltaMainStackMapEncoder::enumerate);
  if (AP.TM.getTargetTriple().getArch() != Triple::aarch64)
    report_fatal_error(
        "the \"kotlin-native\" GC strategy's delta-main stack-map format is "
        "only implemented for AArch64 in this build");

  // Nothing to do if no callsite was ever recorded.
  if (SM.getCSInfos().empty())
    return true;

  DeltaMainStackMapEncoder Encoder;
  deltamain::EncodedStackMap Map = Encoder.build(SM);

  MCContext &Ctx = AP.OutStreamer->getContext();
  MCSection *Section = Ctx.getObjectFileInfo()->getStackMapSection();
  // In the lazy mode a function refers to its own record, so the linker can
  // remove the records of the removed functions. The eager mode refers to no
  // record, they must stay.
  if (KNLazyStackMap)
    if (auto *MachOSection = dyn_cast<MCSectionMachO>(Section))
      MachOSection->clearAttribute(MachO::S_ATTR_NO_DEAD_STRIP);
  AP.OutStreamer->switchSection(Section);

  Encoder.emit(*AP.OutStreamer, Map);

  return true;
}
