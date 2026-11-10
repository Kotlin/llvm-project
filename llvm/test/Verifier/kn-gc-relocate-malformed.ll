; A kn.gc.relocate whose first argument is not tied to a landingpad is malformed
; IR. The verifier must report it with a diagnostic and not crash.
;
; XFAIL: *
; Today the verifier finds the problem, but printing the failing instruction asks
; GCRelocateInst for its base pointer, which calls GCProjectionInst::getStatepoint()
; and that asserts (extractvalue of a non-landingpad) or casts a non-instruction
; operand. Remove the XFAIL when getStatepoint() tolerates such operands.

; RUN: rm -rf %t && split-file %s %t
; RUN: not opt -passes=verify -disable-output %t/not-from-landingpad.ll 2>&1 | FileCheck %t/not-from-landingpad.ll
; RUN: not opt -passes=verify -disable-output %t/extractvalue-not-from-landingpad.ll 2>&1 | FileCheck %t/extractvalue-not-from-landingpad.ll

;--- not-from-landingpad.ll
; The first argument is an arbitrary i32, it is not tied to any statepoint.
declare ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32, i32, i32)

define ptr addrspace(1) @f(i32 %x) gc "kotlin-native" {
  %r = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %x, i32 0, i32 0)
  ret ptr addrspace(1) %r
}
; CHECK: gc relocate is incorrectly tied to the statepoint


;--- extractvalue-not-from-landingpad.ll
; extractvalue is only accepted when it reads a landingpad.
declare ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32, i32, i32)

define ptr addrspace(1) @f({ ptr, i32 } %agg) gc "kotlin-native" {
  %sel = extractvalue { ptr, i32 } %agg, 1
  %r = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %sel, i32 0, i32 0)
  ret ptr addrspace(1) %r
}
; CHECK: gc relocate is incorrectly tied to the statepoint
