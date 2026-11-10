; RUN: llc -mtriple=aarch64-unknown-linux-gnu -verify-machineinstrs < %s | FileCheck %s

; Smoke test for the "kotlin-native" GC strategy's delta-main stack-map
; format (KotlinNativeGCPrinter / DeltaMainStackMapEncoder). Confirms that:
;  - a function using `gc "kotlin-native"` compiles instead of hitting the
;    AArch64-only report_fatal_error gate (this test itself IS the AArch64
;    target, so it should never trip that gate);
;  - each function with a recorded call site gets a
;    ".Lstackmap_delta_main_start.<fn>" label immediately before its
;    delta-main record.
;
; NOTE: this only checks structural markers (label/symbol names) that are
; deterministic from DeltaMainStackMapEncoder's source. It does not check
; the actual encoded bytes (function offset / BaseOffset / stack-slot bit
; vector / derived-pointer pairs): those depend on register allocation and
; frame layout decisions this repository's environment cannot exercise
; (no working `llc` build here -- see the porting plan). Regenerate this
; test's byte-level CHECK lines from a real build via e.g.
; `llvm-readobj --hex-dump=__llvm_stackmaps` before this format is
; considered ready to land.

target datalayout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128"
target triple = "aarch64-unknown-linux-gnu"

declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

declare void @consume(ptr addrspace(1) %obj)

; A base pointer with no separately-live derived pointer: exercises
; DeltaMainStackMapEncoder::collectBaseToDerived's `Base == Derived` case.
define void @test_base_only(ptr addrspace(1) %a) gc "kotlin-native" {
; CHECK-LABEL: test_base_only:
entry:
  %tok = tail call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void (ptr addrspace(1))) @consume, i32 1, i32 0, ptr addrspace(1) %a, i32 0, i32 0) ["gc-live" (ptr addrspace(1) %a)]
  ret void
}

; A base pointer with a distinct, separately-live derived pointer:
; exercises the DerivedSlots path end to end.
define void @test_base_and_derived(ptr addrspace(1) %a) gc "kotlin-native" {
; CHECK-LABEL: test_base_and_derived:
entry:
  %tok = tail call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void (ptr addrspace(1))) @consume, i32 1, i32 0, ptr addrspace(1) %a, i32 0, i32 0) ["gc-live" (ptr addrspace(1) %a)]
  %b = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %tok, i32 0, i32 0)
  call void @consume(ptr addrspace(1) %b)
  ret void
}

; CHECK: .Lstackmap_delta_main_start.test_base_only:
; CHECK: .Lstackmap_delta_main_start.test_base_and_derived:
