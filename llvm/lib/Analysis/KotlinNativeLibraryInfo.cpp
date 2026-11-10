//===-- KotlinNativeLibraryInfo.cpp - Kotlin Native Library information ----==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the KotlinNativeLibraryInfo registry.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/KotlinNativeLibraryInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include <array>
using namespace llvm;

static StringLiteral const KotlinNativeLibFuncNames[NumKotlinNativeLibFuncs] =
    {
#define KNLI_DEFINE_STRING
#include "llvm/Analysis/KotlinNativeLibraryInfo.def"
};

// Recognized types of registry function arguments and return types. This
// mirrors (a subset of) TargetLibraryInfo.cpp's FuncArgTypeID, but the match
// below is deliberately looser: the real safety net for this registry is the
// exact-name match against a small, hand-verified list, not a strict
// signature check.
enum KotlinNativeFuncArgTypeID : char {
  Void = 0, // Must be zero.
  Bool,     // A C++ bool parameter: Clang's ABI lowering represents these as
            // i1 (with a zeroext attribute) in a function's parameter list,
            // not i8 (i8 does show up for bool in other contexts, e.g. struct
            // fields or arrays, just not here).
  Int32,
  Long,     // size_t-like; 32 or 64 bits.
  Ptr,      // Any pointer type.
  Struct,   // Any struct type, or the array-coerced form Clang's ABI
            // lowering produces for small by-value aggregates (e.g. the
            // [2 x i64] it emits for std_support::span<char> by value).
  Ellip,    // The ellipsis (...).
  Any,      // Matches any type, including void; used for return types that
            // may be optimized away (e.g. dead-return-value elimination).
};

typedef std::array<KotlinNativeFuncArgTypeID, 8> KotlinNativeFuncProtoTy;

static const KotlinNativeFuncProtoTy KotlinNativeSignatures[] = {
#define KNLI_DEFINE_SIG
#include "llvm/Analysis/KotlinNativeLibraryInfo.def"
};

static_assert(sizeof KotlinNativeSignatures / sizeof *KotlinNativeSignatures ==
                  NumKotlinNativeLibFuncs,
              "Missing Kotlin/Native library function signatures");

static bool matchType(KotlinNativeFuncArgTypeID ArgTy, const Type *Ty) {
  switch (ArgTy) {
  case Void:
    return Ty->isVoidTy();
  case Bool:
    return Ty->isIntegerTy(1) || Ty->isIntegerTy(8);
  case Int32:
    return Ty->isIntegerTy(32);
  case Long:
    return Ty->isIntegerTy() && Ty->getPrimitiveSizeInBits() >= 32;
  case Ptr:
    return Ty->isPointerTy();
  case Struct:
    return Ty->isStructTy() || Ty->isArrayTy();
  case Any:
    return true;
  default:
    break;
  }

  llvm_unreachable("Invalid type");
}

static bool isValidProtoForKotlinNativeLibFunc(const FunctionType &FTy,
                                               KotlinNativeLibFunc F) {
  unsigned NumParams = FTy.getNumParams();

  unsigned Idx = 0;
  Type *Ty = FTy.getReturnType();
  const auto &ProtoTypes = KotlinNativeSignatures[F];
  for (auto TyID : ProtoTypes) {
    if (Idx && TyID == Void)
      break;

    if (TyID == Ellip) {
      assert(Idx == ProtoTypes.size() - 1 || ProtoTypes[Idx + 1] == Void);
      return FTy.isFunctionVarArg();
    }

    if (!Ty || !matchType(TyID, Ty))
      return false;

    if (Idx == NumParams) {
      Ty = nullptr;
      ++Idx;
      continue;
    }

    Ty = FTy.getParamType(Idx++);
  }

  return Idx == NumParams + 1 && !FTy.isFunctionVarArg();
}

static DenseMap<StringRef, KotlinNativeLibFunc> buildIndexMap() {
  DenseMap<StringRef, KotlinNativeLibFunc> Indices;
  Indices.reserve(NumKotlinNativeLibFuncs);
  unsigned Idx = 0;
  for (const auto &Name : KotlinNativeLibFuncNames)
    Indices[Name] = static_cast<KotlinNativeLibFunc>(Idx++);
  return Indices;
}

bool llvm::getKotlinNativeLibFunc(StringRef FuncName, KotlinNativeLibFunc &F) {
  if (FuncName.empty())
    return false;

  static const DenseMap<StringRef, KotlinNativeLibFunc> Indices =
      buildIndexMap();

  if (auto Loc = Indices.find(FuncName); Loc != Indices.end()) {
    F = Loc->second;
    return true;
  }
  return false;
}

bool llvm::getKotlinNativeLibFunc(const Function &FDecl,
                                  KotlinNativeLibFunc &F) {
  if (FDecl.isIntrinsic())
    return false;

  if (!getKotlinNativeLibFunc(FDecl.getName(), F))
    return false;

  return isValidProtoForKotlinNativeLibFunc(*FDecl.getFunctionType(), F);
}

bool llvm::getKotlinNativeLibFunc(const CallBase &CB, KotlinNativeLibFunc &F) {
  if (CB.isNoBuiltin())
    return false;

  // getCalledFunction() is null if the type of the call differs from the type of the declaration.
  const auto *Callee = dyn_cast<Function>(CB.getCalledOperand());
  if (!Callee)
    return false;
  const FunctionType *CallTy = CB.getFunctionType();
  if (CallTy != Callee->getFunctionType() && !CallTy->isVarArg())
    return false;
  return getKotlinNativeLibFunc(*Callee, F);
}
