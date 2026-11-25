; RUN: opt < %s -S -passes=rewrite-statepoints-for-gc,verify | FileCheck %s

; On the exceptional path of an invoke statepoint there is no real statepoint
; token, so the landingpad stands in for it.
;  * A `landingpad token` (the upstream convention) is passed to
;    llvm.experimental.gc.relocate directly.
;  * A landingpad with a fixed non-token type (the `{ ptr, i32 }` that
;    Kotlin/Native emits) cannot be, so its i32 selector is passed to
;    llvm.experimental.kn.gc.relocate instead.

declare void @foo()
declare i32 @personality()

; Kotlin/Native: the landingpad result is also used by the frontend itself
; (the selector is compared against the Kotlin exception type id), so the
; foreign `extractvalue` must not be mistaken for a relocate.
; CHECK-LABEL: @test_kn(
define ptr addrspace(1) @test_kn(ptr addrspace(1) %a, ptr addrspace(1) %b) gc "kotlin-native" personality ptr @personality {
entry:
  invoke void @foo() to label %normal unwind label %lpad

normal:
  ret ptr addrspace(1) null

; CHECK: lpad:
; CHECK: [[LP:%.*]] = landingpad { ptr, i32 }
; CHECK-DAG: extractvalue { ptr, i32 } [[LP]], 1
; CHECK-DAG: %a.relocated = call coldcc ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 {{%.*}}, i32 {{[0-9]+}}, i32 {{[0-9]+}})
; CHECK-DAG: %b.relocated = call coldcc ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 {{%.*}}, i32 {{[0-9]+}}, i32 {{[0-9]+}})
; CHECK-NOT: @llvm.experimental.gc.relocate
; The CHECK-NOT above is only checked up to this line; without a closing
; CHECK it would extend to the intrinsic declarations at the end of the module.
; CHECK: ret ptr addrspace(1)
lpad:
  %lp = landingpad { ptr, i32 }
          cleanup
  %sel = extractvalue { ptr, i32 } %lp, 1
  %is.kotlin = icmp eq i32 %sel, 0
  %r = select i1 %is.kotlin, ptr addrspace(1) %a, ptr addrspace(1) %b
  ret ptr addrspace(1) %r
}

; Upstream convention: `landingpad token` keeps using the ordinary
; gc.relocate with the landingpad as its token operand.
; CHECK-LABEL: @test_token(
define ptr addrspace(1) @test_token(ptr addrspace(1) %a, ptr addrspace(1) %b) gc "statepoint-example" personality ptr @personality {
entry:
  invoke void @foo() to label %normal unwind label %lpad

normal:
  ret ptr addrspace(1) null

; CHECK: lpad:
; CHECK: [[LP:%.*]] = landingpad token
; CHECK-DAG: %a.relocated = call coldcc ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token [[LP]], i32 {{[0-9]+}}, i32 {{[0-9]+}})
; CHECK-DAG: %b.relocated = call coldcc ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token [[LP]], i32 {{[0-9]+}}, i32 {{[0-9]+}})
; CHECK-NOT: kn.gc.relocate
; CHECK-NOT: extractvalue
; CHECK: ret ptr addrspace(1)
lpad:
  %lp = landingpad token
          cleanup
  %c = icmp eq ptr addrspace(1) %a, %b
  %r = select i1 %c, ptr addrspace(1) %a, ptr addrspace(1) %b
  ret ptr addrspace(1) %r
}
