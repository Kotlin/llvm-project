; REQUIRES: x86-registered-target

; The delta-main stack map encoder works only for AArch64 (it depends on the
; AArch64 frame layout). A "kotlin-native" statepoint on another architecture
; must stop the compiler with a clear message, not write a wrong stack map.

; RUN: not --crash llc -mtriple=x86_64-unknown-linux-gnu %s -o /dev/null 2>&1 | FileCheck %s

; CHECK: LLVM ERROR: the "kotlin-native" GC strategy's delta-main stack-map format is only implemented for AArch64

declare void @foo()
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)

define void @f() gc "kotlin-native" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0)
  ret void
}
