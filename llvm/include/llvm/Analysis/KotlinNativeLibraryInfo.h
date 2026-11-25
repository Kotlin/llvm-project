//===-- KotlinNativeLibraryInfo.h - Kotlin Native Library information ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides a small, closed registry of Kotlin/Native runtime
// helper functions that are known to never touch Kotlin objects, allocate,
// or take a safepoint ("GC leaf" functions), analogous to how
// TargetLibraryInfo recognizes libc functions. It is intentionally separate
// from TargetLibraryInfo so that this narrow, hand-verified list does not
// affect any other optimization that consults the real libc registry.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_KOTLINNATIVELIBRARYINFO_H
#define LLVM_ANALYSIS_KOTLINNATIVELIBRARYINFO_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/InstrTypes.h"

namespace llvm {

class CallBase;
class Function;
class FunctionType;

enum KotlinNativeLibFunc : unsigned {
#define KNLI_DEFINE_ENUM
#include "llvm/Analysis/KotlinNativeLibraryInfo.def"

  NumKotlinNativeLibFuncs,
  NotKotlinNativeLibFunc
};

/// Searches for a particular function name in the Kotlin/Native leaf-function
/// registry.
///
/// If it is one of the known functions, return true and set F to the
/// corresponding value.
bool getKotlinNativeLibFunc(StringRef FuncName, KotlinNativeLibFunc &F);

/// Searches for a particular function name, also checking that its type is
/// (approximately) valid for the matching registry entry.
///
/// If it is one of the known functions, return true and set F to the
/// corresponding value.
bool getKotlinNativeLibFunc(const Function &FDecl, KotlinNativeLibFunc &F);

/// If a call's callee is a known Kotlin/Native leaf function, return true and
/// set F to that function.
bool getKotlinNativeLibFunc(const CallBase &CB, KotlinNativeLibFunc &F);

} // end namespace llvm

#endif
