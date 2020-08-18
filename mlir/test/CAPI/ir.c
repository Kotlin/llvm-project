/*===- ir.c - Simple test of C APIs ---------------------------------------===*\
|*                                                                            *|
|* Part of the LLVM Project, under the Apache License v2.0 with LLVM          *|
|* Exceptions.                                                                *|
|* See https://llvm.org/LICENSE.txt for license information.                  *|
|* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception                    *|
|*                                                                            *|
\*===----------------------------------------------------------------------===*/

/* RUN: mlir-capi-ir-test 2>&1 | FileCheck %s
 */

#include "mlir-c/IR.h"
#include "mlir-c/Registration.h"
#include "mlir-c/StandardTypes.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void populateLoopBody(MlirContext ctx, MlirBlock loopBody,
                      MlirLocation location, MlirBlock funcBody) {
  MlirValue iv = mlirBlockGetArgument(loopBody, 0);
  MlirValue funcArg0 = mlirBlockGetArgument(funcBody, 0);
  MlirValue funcArg1 = mlirBlockGetArgument(funcBody, 1);
  MlirType f32Type = mlirTypeParseGet(ctx, "f32");

  MlirOperationState loadLHSState = mlirOperationStateGet("std.load", location);
  MlirValue loadLHSOperands[] = {funcArg0, iv};
  mlirOperationStateAddOperands(&loadLHSState, 2, loadLHSOperands);
  mlirOperationStateAddResults(&loadLHSState, 1, &f32Type);
  MlirOperation loadLHS = mlirOperationCreate(&loadLHSState);
  mlirBlockAppendOwnedOperation(loopBody, loadLHS);

  MlirOperationState loadRHSState = mlirOperationStateGet("std.load", location);
  MlirValue loadRHSOperands[] = {funcArg1, iv};
  mlirOperationStateAddOperands(&loadRHSState, 2, loadRHSOperands);
  mlirOperationStateAddResults(&loadRHSState, 1, &f32Type);
  MlirOperation loadRHS = mlirOperationCreate(&loadRHSState);
  mlirBlockAppendOwnedOperation(loopBody, loadRHS);

  MlirOperationState addState = mlirOperationStateGet("std.addf", location);
  MlirValue addOperands[] = {mlirOperationGetResult(loadLHS, 0),
                             mlirOperationGetResult(loadRHS, 0)};
  mlirOperationStateAddOperands(&addState, 2, addOperands);
  mlirOperationStateAddResults(&addState, 1, &f32Type);
  MlirOperation add = mlirOperationCreate(&addState);
  mlirBlockAppendOwnedOperation(loopBody, add);

  MlirOperationState storeState = mlirOperationStateGet("std.store", location);
  MlirValue storeOperands[] = {mlirOperationGetResult(add, 0), funcArg0, iv};
  mlirOperationStateAddOperands(&storeState, 3, storeOperands);
  MlirOperation store = mlirOperationCreate(&storeState);
  mlirBlockAppendOwnedOperation(loopBody, store);

  MlirOperationState yieldState = mlirOperationStateGet("scf.yield", location);
  MlirOperation yield = mlirOperationCreate(&yieldState);
  mlirBlockAppendOwnedOperation(loopBody, yield);
}

MlirModule makeAdd(MlirContext ctx, MlirLocation location) {
  MlirModule moduleOp = mlirModuleCreateEmpty(location);
  MlirOperation module = mlirModuleGetOperation(moduleOp);
  MlirRegion moduleBodyRegion = mlirOperationGetRegion(module, 0);
  MlirBlock moduleBody = mlirRegionGetFirstBlock(moduleBodyRegion);

  MlirType memrefType = mlirTypeParseGet(ctx, "memref<?xf32>");
  MlirType funcBodyArgTypes[] = {memrefType, memrefType};
  MlirRegion funcBodyRegion = mlirRegionCreate();
  MlirBlock funcBody = mlirBlockCreate(
      sizeof(funcBodyArgTypes) / sizeof(MlirType), funcBodyArgTypes);
  mlirRegionAppendOwnedBlock(funcBodyRegion, funcBody);

  MlirAttribute funcTypeAttr =
      mlirAttributeParseGet(ctx, "(memref<?xf32>, memref<?xf32>) -> ()");
  MlirAttribute funcNameAttr = mlirAttributeParseGet(ctx, "\"add\"");
  MlirNamedAttribute funcAttrs[] = {
      mlirNamedAttributeGet("type", funcTypeAttr),
      mlirNamedAttributeGet("sym_name", funcNameAttr)};
  MlirOperationState funcState = mlirOperationStateGet("func", location);
  mlirOperationStateAddAttributes(&funcState, 2, funcAttrs);
  mlirOperationStateAddOwnedRegions(&funcState, 1, &funcBodyRegion);
  MlirOperation func = mlirOperationCreate(&funcState);
  mlirBlockInsertOwnedOperation(moduleBody, 0, func);

  MlirType indexType = mlirTypeParseGet(ctx, "index");
  MlirAttribute indexZeroLiteral = mlirAttributeParseGet(ctx, "0 : index");
  MlirNamedAttribute indexZeroValueAttr =
      mlirNamedAttributeGet("value", indexZeroLiteral);
  MlirOperationState constZeroState =
      mlirOperationStateGet("std.constant", location);
  mlirOperationStateAddResults(&constZeroState, 1, &indexType);
  mlirOperationStateAddAttributes(&constZeroState, 1, &indexZeroValueAttr);
  MlirOperation constZero = mlirOperationCreate(&constZeroState);
  mlirBlockAppendOwnedOperation(funcBody, constZero);

  MlirValue funcArg0 = mlirBlockGetArgument(funcBody, 0);
  MlirValue constZeroValue = mlirOperationGetResult(constZero, 0);
  MlirValue dimOperands[] = {funcArg0, constZeroValue};
  MlirOperationState dimState = mlirOperationStateGet("std.dim", location);
  mlirOperationStateAddOperands(&dimState, 2, dimOperands);
  mlirOperationStateAddResults(&dimState, 1, &indexType);
  MlirOperation dim = mlirOperationCreate(&dimState);
  mlirBlockAppendOwnedOperation(funcBody, dim);

  MlirRegion loopBodyRegion = mlirRegionCreate();
  MlirBlock loopBody = mlirBlockCreate(/*nArgs=*/1, &indexType);
  mlirRegionAppendOwnedBlock(loopBodyRegion, loopBody);

  MlirAttribute indexOneLiteral = mlirAttributeParseGet(ctx, "1 : index");
  MlirNamedAttribute indexOneValueAttr =
      mlirNamedAttributeGet("value", indexOneLiteral);
  MlirOperationState constOneState =
      mlirOperationStateGet("std.constant", location);
  mlirOperationStateAddResults(&constOneState, 1, &indexType);
  mlirOperationStateAddAttributes(&constOneState, 1, &indexOneValueAttr);
  MlirOperation constOne = mlirOperationCreate(&constOneState);
  mlirBlockAppendOwnedOperation(funcBody, constOne);

  MlirValue dimValue = mlirOperationGetResult(dim, 0);
  MlirValue constOneValue = mlirOperationGetResult(constOne, 0);
  MlirValue loopOperands[] = {constZeroValue, dimValue, constOneValue};
  MlirOperationState loopState = mlirOperationStateGet("scf.for", location);
  mlirOperationStateAddOperands(&loopState, 3, loopOperands);
  mlirOperationStateAddOwnedRegions(&loopState, 1, &loopBodyRegion);
  MlirOperation loop = mlirOperationCreate(&loopState);
  mlirBlockAppendOwnedOperation(funcBody, loop);

  populateLoopBody(ctx, loopBody, location, funcBody);

  MlirOperationState retState = mlirOperationStateGet("std.return", location);
  MlirOperation ret = mlirOperationCreate(&retState);
  mlirBlockAppendOwnedOperation(funcBody, ret);

  return moduleOp;
}

struct OpListNode {
  MlirOperation op;
  struct OpListNode *next;
};
typedef struct OpListNode OpListNode;

struct ModuleStats {
  unsigned numOperations;
  unsigned numAttributes;
  unsigned numBlocks;
  unsigned numRegions;
  unsigned numValues;
};
typedef struct ModuleStats ModuleStats;

void collectStatsSingle(OpListNode *head, ModuleStats *stats) {
  MlirOperation operation = head->op;
  stats->numOperations += 1;
  stats->numValues += mlirOperationGetNumResults(operation);
  stats->numAttributes += mlirOperationGetNumAttributes(operation);

  unsigned numRegions = mlirOperationGetNumRegions(operation);

  stats->numRegions += numRegions;

  for (unsigned i = 0; i < numRegions; ++i) {
    MlirRegion region = mlirOperationGetRegion(operation, i);
    for (MlirBlock block = mlirRegionGetFirstBlock(region);
         !mlirBlockIsNull(block); block = mlirBlockGetNextInRegion(block)) {
      ++stats->numBlocks;
      stats->numValues += mlirBlockGetNumArguments(block);

      for (MlirOperation child = mlirBlockGetFirstOperation(block);
           !mlirOperationIsNull(child);
           child = mlirOperationGetNextInBlock(child)) {
        OpListNode *node = malloc(sizeof(OpListNode));
        node->op = child;
        node->next = head->next;
        head->next = node;
      }
    }
  }
}

void collectStats(MlirOperation operation) {
  OpListNode *head = malloc(sizeof(OpListNode));
  head->op = operation;
  head->next = NULL;

  ModuleStats stats;
  stats.numOperations = 0;
  stats.numAttributes = 0;
  stats.numBlocks = 0;
  stats.numRegions = 0;
  stats.numValues = 0;

  do {
    collectStatsSingle(head, &stats);
    OpListNode *next = head->next;
    free(head);
    head = next;
  } while (head);

  fprintf(stderr, "Number of operations: %u\n", stats.numOperations);
  fprintf(stderr, "Number of attributes: %u\n", stats.numAttributes);
  fprintf(stderr, "Number of blocks: %u\n", stats.numBlocks);
  fprintf(stderr, "Number of regions: %u\n", stats.numRegions);
  fprintf(stderr, "Number of values: %u\n", stats.numValues);
}

static void printToStderr(const char *str, intptr_t len, void *userData) {
  (void)userData;
  fwrite(str, 1, len, stderr);
}

static void printFirstOfEach(MlirOperation operation) {
  // Assuming we are given a module, go to the first operation of the first
  // function.
  MlirRegion region = mlirOperationGetRegion(operation, 0);
  MlirBlock block = mlirRegionGetFirstBlock(region);
  operation = mlirBlockGetFirstOperation(block);
  region = mlirOperationGetRegion(operation, 0);
  block = mlirRegionGetFirstBlock(region);
  operation = mlirBlockGetFirstOperation(block);

  // In the module we created, the first operation of the first function is an
  // "std.dim", which has an attribute an a single result that we can use to
  // test the printing mechanism.
  mlirBlockPrint(block, printToStderr, NULL);
  fprintf(stderr, "\n");
  mlirOperationPrint(operation, printToStderr, NULL);
  fprintf(stderr, "\n");

  MlirNamedAttribute namedAttr = mlirOperationGetAttribute(operation, 0);
  mlirAttributePrint(namedAttr.attribute, printToStderr, NULL);
  fprintf(stderr, "\n");

  MlirValue value = mlirOperationGetResult(operation, 0);
  mlirValuePrint(value, printToStderr, NULL);
  fprintf(stderr, "\n");

  MlirType type = mlirValueGetType(value);
  mlirTypePrint(type, printToStderr, NULL);
  fprintf(stderr, "\n");
}

/// Dumps instances of all standard types to check that C API works correctly.
/// Additionally, performs simple identity checks that a standard type
/// constructed with C API can be inspected and has the expected type. The
/// latter achieves full coverage of C API for standard types. Returns 0 on
/// success and a non-zero error code on failure.
static int printStandardTypes(MlirContext ctx) {
  // Integer types.
  MlirType i32 = mlirIntegerTypeGet(ctx, 32);
  MlirType si32 = mlirIntegerTypeSignedGet(ctx, 32);
  MlirType ui32 = mlirIntegerTypeUnsignedGet(ctx, 32);
  if (!mlirTypeIsAInteger(i32) || mlirTypeIsAF32(i32))
    return 1;
  if (!mlirTypeIsAInteger(si32) || !mlirIntegerTypeIsSigned(si32))
    return 2;
  if (!mlirTypeIsAInteger(ui32) || !mlirIntegerTypeIsUnsigned(ui32))
    return 3;
  if (mlirTypeEqual(i32, ui32) || mlirTypeEqual(i32, si32))
    return 4;
  if (mlirIntegerTypeGetWidth(i32) != mlirIntegerTypeGetWidth(si32))
    return 5;
  mlirTypeDump(i32);
  fprintf(stderr, "\n");
  mlirTypeDump(si32);
  fprintf(stderr, "\n");
  mlirTypeDump(ui32);
  fprintf(stderr, "\n");

  // Index type.
  MlirType index = mlirIndexTypeGet(ctx);
  if (!mlirTypeIsAIndex(index))
    return 6;
  mlirTypeDump(index);
  fprintf(stderr, "\n");

  // Floating-point types.
  MlirType bf16 = mlirBF16TypeGet(ctx);
  MlirType f16 = mlirF16TypeGet(ctx);
  MlirType f32 = mlirF32TypeGet(ctx);
  MlirType f64 = mlirF64TypeGet(ctx);
  if (!mlirTypeIsABF16(bf16))
    return 7;
  if (!mlirTypeIsAF16(f16))
    return 9;
  if (!mlirTypeIsAF32(f32))
    return 10;
  if (!mlirTypeIsAF64(f64))
    return 11;
  mlirTypeDump(bf16);
  fprintf(stderr, "\n");
  mlirTypeDump(f16);
  fprintf(stderr, "\n");
  mlirTypeDump(f32);
  fprintf(stderr, "\n");
  mlirTypeDump(f64);
  fprintf(stderr, "\n");

  // None type.
  MlirType none = mlirNoneTypeGet(ctx);
  if (!mlirTypeIsANone(none))
    return 12;
  mlirTypeDump(none);
  fprintf(stderr, "\n");

  // Complex type.
  MlirType cplx = mlirComplexTypeGet(f32);
  if (!mlirTypeIsAComplex(cplx) ||
      !mlirTypeEqual(mlirComplexTypeGetElementType(cplx), f32))
    return 13;
  mlirTypeDump(cplx);
  fprintf(stderr, "\n");

  // Vector (and Shaped) type. ShapedType is a common base class for vectors,
  // memrefs and tensors, one cannot create instances of this class so it is
  // tested on an instance of vector type.
  int64_t shape[] = {2, 3};
  MlirType vector =
      mlirVectorTypeGet(sizeof(shape) / sizeof(int64_t), shape, f32);
  if (!mlirTypeIsAVector(vector) || !mlirTypeIsAShaped(vector))
    return 14;
  if (!mlirTypeEqual(mlirShapedTypeGetElementType(vector), f32) ||
      !mlirShapedTypeHasRank(vector) || mlirShapedTypeGetRank(vector) != 2 ||
      mlirShapedTypeGetDimSize(vector, 0) != 2 ||
      mlirShapedTypeIsDynamicDim(vector, 0) ||
      mlirShapedTypeGetDimSize(vector, 1) != 3 ||
      !mlirShapedTypeHasStaticShape(vector))
    return 15;
  mlirTypeDump(vector);
  fprintf(stderr, "\n");

  // Ranked tensor type.
  MlirType rankedTensor =
      mlirRankedTensorTypeGet(sizeof(shape) / sizeof(int64_t), shape, f32);
  if (!mlirTypeIsATensor(rankedTensor) ||
      !mlirTypeIsARankedTensor(rankedTensor))
    return 16;
  mlirTypeDump(rankedTensor);
  fprintf(stderr, "\n");

  // Unranked tensor type.
  MlirType unrankedTensor = mlirUnrankedTensorTypeGet(f32);
  if (!mlirTypeIsATensor(unrankedTensor) ||
      !mlirTypeIsAUnrankedTensor(unrankedTensor) ||
      mlirShapedTypeHasRank(unrankedTensor))
    return 17;
  mlirTypeDump(unrankedTensor);
  fprintf(stderr, "\n");

  // MemRef type.
  MlirType memRef = mlirMemRefTypeContiguousGet(
      f32, sizeof(shape) / sizeof(int64_t), shape, 2);
  if (!mlirTypeIsAMemRef(memRef) ||
      mlirMemRefTypeGetNumAffineMaps(memRef) != 0 ||
      mlirMemRefTypeGetMemorySpace(memRef) != 2)
    return 18;
  mlirTypeDump(memRef);
  fprintf(stderr, "\n");

  // Unranked MemRef type.
  MlirType unrankedMemRef = mlirUnrankedMemRefTypeGet(f32, 4);
  if (!mlirTypeIsAUnrankedMemRef(unrankedMemRef) ||
      mlirTypeIsAMemRef(unrankedMemRef) ||
      mlirUnrankedMemrefGetMemorySpace(unrankedMemRef) != 4)
    return 19;
  mlirTypeDump(unrankedMemRef);
  fprintf(stderr, "\n");

  // Tuple type.
  MlirType types[] = {unrankedMemRef, f32};
  MlirType tuple = mlirTupleTypeGet(ctx, 2, types);
  if (!mlirTypeIsATuple(tuple) || mlirTupleTypeGetNumTypes(tuple) != 2 ||
      !mlirTypeEqual(mlirTupleTypeGetType(tuple, 0), unrankedMemRef) ||
      !mlirTypeEqual(mlirTupleTypeGetType(tuple, 1), f32))
    return 20;
  mlirTypeDump(tuple);
  fprintf(stderr, "\n");

  return 0;
}

int main() {
  mlirRegisterAllDialects();
  MlirContext ctx = mlirContextCreate();
  mlirContextLoadAllDialects(ctx);
  MlirLocation location = mlirLocationUnknownGet(ctx);

  MlirModule moduleOp = makeAdd(ctx, location);
  MlirOperation module = mlirModuleGetOperation(moduleOp);
  mlirOperationDump(module);
  // clang-format off
  // CHECK: module {
  // CHECK:   func @add(%[[ARG0:.*]]: memref<?xf32>, %[[ARG1:.*]]: memref<?xf32>) {
  // CHECK:     %[[C0:.*]] = constant 0 : index
  // CHECK:     %[[DIM:.*]] = dim %[[ARG0]], %[[C0]] : memref<?xf32>
  // CHECK:     %[[C1:.*]] = constant 1 : index
  // CHECK:     scf.for %[[I:.*]] = %[[C0]] to %[[DIM]] step %[[C1]] {
  // CHECK:       %[[LHS:.*]] = load %[[ARG0]][%[[I]]] : memref<?xf32>
  // CHECK:       %[[RHS:.*]] = load %[[ARG1]][%[[I]]] : memref<?xf32>
  // CHECK:       %[[SUM:.*]] = addf %[[LHS]], %[[RHS]] : f32
  // CHECK:       store %[[SUM]], %[[ARG0]][%[[I]]] : memref<?xf32>
  // CHECK:     }
  // CHECK:     return
  // CHECK:   }
  // CHECK: }
  // clang-format on

  collectStats(module);
  // clang-format off
  // CHECK: Number of operations: 13
  // CHECK: Number of attributes: 4
  // CHECK: Number of blocks: 3
  // CHECK: Number of regions: 3
  // CHECK: Number of values: 9
  // clang-format on

  printFirstOfEach(module);
  // clang-format off
  // CHECK:   %[[C0:.*]] = constant 0 : index
  // CHECK:   %[[DIM:.*]] = dim %{{.*}}, %[[C0]] : memref<?xf32>
  // CHECK:   %[[C1:.*]] = constant 1 : index
  // CHECK:   scf.for %[[I:.*]] = %[[C0]] to %[[DIM]] step %[[C1]] {
  // CHECK:     %[[LHS:.*]] = load %{{.*}}[%[[I]]] : memref<?xf32>
  // CHECK:     %[[RHS:.*]] = load %{{.*}}[%[[I]]] : memref<?xf32>
  // CHECK:     %[[SUM:.*]] = addf %[[LHS]], %[[RHS]] : f32
  // CHECK:     store %[[SUM]], %{{.*}}[%[[I]]] : memref<?xf32>
  // CHECK:   }
  // CHECK: return
  // CHECK: constant 0 : index
  // CHECK: 0 : index
  // CHECK: constant 0 : index
  // CHECK: index
  // clang-format on

  mlirModuleDestroy(moduleOp);

  // clang-format off
  // CHECK-LABEL: @types
  // CHECK: i32
  // CHECK: si32
  // CHECK: ui32
  // CHECK: index
  // CHECK: bf16
  // CHECK: f16
  // CHECK: f32
  // CHECK: f64
  // CHECK: none
  // CHECK: complex<f32>
  // CHECK: vector<2x3xf32>
  // CHECK: tensor<2x3xf32>
  // CHECK: tensor<*xf32>
  // CHECK: memref<2x3xf32, 2>
  // CHECK: memref<*xf32, 4>
  // CHECK: tuple<memref<*xf32, 4>, f32>
  // CHECK: 0
  // clang-format on
  fprintf(stderr, "@types");
  int errcode = printStandardTypes(ctx);
  fprintf(stderr, "%d\n", errcode);

  mlirContextDestroy(ctx);

  return 0;
}
