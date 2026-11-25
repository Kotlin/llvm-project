; RUN: opt < %s -S -passes='simplifycfg<sink-common-insts>' | FileCheck %s
; RUN: opt < %s -S -passes='simplifycfg<sink-common-insts>,verify' -disable-output

; A relocate must stay in the landing pad block of its statepoint: its first argument is
; tied to that landing pad. Two landing pads with the same sequence of instructions and the
; same successor are a candidate for sinking the common code into the successor, but the
; relocate must not move there.

declare void @foo()
declare void @bar()
declare i32 @personality()
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)
declare ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32, i32, i32)
declare void @use(ptr addrspace(1))

define void @two_landing_pads(ptr addrspace(1) %a) gc "kotlin-native" personality ptr @personality {
entry:
  %t1 = invoke token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
          to label %second unwind label %lpad1

second:
  %t2 = invoke token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @bar, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
          to label %done unwind label %lpad2

done:
  ret void

; CHECK-LABEL: lpad1:
; CHECK: landingpad
; CHECK: @llvm.experimental.kn.gc.relocate.p1(
; CHECK-LABEL: lpad2:
; CHECK: landingpad
; CHECK: @llvm.experimental.kn.gc.relocate.p1(
; CHECK-LABEL: cleanup:
; CHECK-NOT: kn.gc.relocate
lpad1:
  %lp1 = landingpad { ptr, i32 } cleanup
  %sel1 = extractvalue { ptr, i32 } %lp1, 1
  %r1 = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %sel1, i32 0, i32 0)
  call void @use(ptr addrspace(1) %r1)
  br label %cleanup

lpad2:
  %lp2 = landingpad { ptr, i32 } cleanup
  %sel2 = extractvalue { ptr, i32 } %lp2, 1
  %r2 = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %sel2, i32 0, i32 0)
  call void @use(ptr addrspace(1) %r2)
  br label %cleanup

cleanup:
  ret void
}
