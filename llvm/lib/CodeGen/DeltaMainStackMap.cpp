//===- DeltaMainStackMap.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/DeltaMainStackMap.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;
using namespace llvm::deltamain;

void Delta::emit(MCStreamer &OS) const {
  OS.emitULEB128BitVector(StackSlots);
}
