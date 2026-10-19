; REQUIRES: aarch64-registered-target
; RUN: opt -passes=globalopt -mtriple=aarch64-unknown-linux-gnu -debug-pass-manager -disable-output < %s 2>&1 | FileCheck %s --check-prefix=ANALYSIS
; RUN: opt -passes=globalopt -mtriple=aarch64-unknown-linux-gnu -S < %s | FileCheck %s --check-prefix=IR
;
; A target that never uses coldcc must not compute cold-call frequencies.
; ANALYSIS: Running pass: GlobalOptPass
; ANALYSIS-NOT: Running analysis: BlockFrequencyAnalysis

declare void @sink(ptr)

define internal void @callee(ptr %p) {
; IR-LABEL: define internal fastcc void @callee(
; IR: call void @sink(ptr %p)
  call void @sink(ptr %p)
  ret void
}

define void @caller(ptr %p, i1 %condition) {
; IR-LABEL: define void @caller(
; IR: call fastcc void @callee(ptr %p)
  br i1 %condition, label %cold, label %exit, !prof !0
cold:
  call void @callee(ptr %p)
  br label %exit
exit:
  ret void
}

!0 = !{!"branch_weights", i32 1, i32 1000}
