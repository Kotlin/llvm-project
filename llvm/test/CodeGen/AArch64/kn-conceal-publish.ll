; RUN: llc -mtriple=aarch64-- < %s | FileCheck %s

; The kn.conceal / kn.publish intrinsics are identity functions at the ISA
; level; codegen must not introduce any copies or calls for them.

define ptr @conceal(ptr addrspace(1) %p) {
; CHECK-LABEL: conceal:
; CHECK:       // %bb.0:
; CHECK-NEXT:    ret
  %r = call ptr @llvm.kn.conceal(ptr addrspace(1) %p)
  ret ptr %r
}

define ptr addrspace(1) @publish(ptr %p) {
; CHECK-LABEL: publish:
; CHECK:       // %bb.0:
; CHECK-NEXT:    ret
  %r = call ptr addrspace(1) @llvm.kn.publish(ptr %p)
  ret ptr addrspace(1) %r
}

define void @store_both(ptr addrspace(1) %p1, ptr %p0) {
; CHECK-LABEL: store_both:
; CHECK:       // %bb.0:
; CHECK-NEXT:    str x0, [x1]
; CHECK-NEXT:    str x1, [x0]
; CHECK-NEXT:    ret
  %c = call ptr @llvm.kn.conceal(ptr addrspace(1) %p1)
  %q = call ptr addrspace(1) @llvm.kn.publish(ptr %p0)
  store ptr %c, ptr %p0
  store ptr addrspace(1) %q, ptr addrspace(1) %p1
  ret void
}

declare ptr @llvm.kn.conceal(ptr addrspace(1))
declare ptr addrspace(1) @llvm.kn.publish(ptr)
