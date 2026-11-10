; Kotlin/Native runs the pass twice on the same module when optimization is on
; (at the end of the default pipeline and again in the LTO pipeline). The second
; run must not change code that is already rewritten, including the exceptional
; path (kn.gc.relocate) and calls that are gc-leaf.

; RUN: opt -S -passes=rewrite-statepoints-for-gc < %s > %t.once.ll
; RUN: opt -S -passes=rewrite-statepoints-for-gc < %t.once.ll > %t.twice.ll
; RUN: diff %t.once.ll %t.twice.ll
; RUN: opt -S -passes=verify < %t.twice.ll -o /dev/null
; RUN: FileCheck %s < %t.twice.ll

declare void @foo()
declare void @leaf() "gc-leaf-function"
declare i32 @personality()
declare ptr addrspace(1) @alloc()

; CHECK-LABEL: @all_shapes(
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK: invoke token {{.*}} @llvm.experimental.gc.statepoint
; CHECK: landingpad { ptr, i32 }
; CHECK: @llvm.experimental.kn.gc.relocate.p1(
define ptr addrspace(1) @all_shapes(ptr addrspace(1) %a, i1 %c) gc "kotlin-native" personality ptr @personality {
entry:
  call void @leaf()
  call void @foo()
  br i1 %c, label %loop, label %call

loop:
  %obj = call ptr addrspace(1) @alloc()
  store i8 1, ptr addrspace(1) %obj
  br label %call

call:
  %p = phi ptr addrspace(1) [ %a, %entry ], [ %obj, %loop ]
  invoke void @foo() to label %normal unwind label %lpad

normal:
  ret ptr addrspace(1) %p

lpad:
  %lp = landingpad { ptr, i32 } cleanup
  %sel = extractvalue { ptr, i32 } %lp, 1
  %cmp = icmp eq i32 %sel, 0
  %r = select i1 %cmp, ptr addrspace(1) %p, ptr addrspace(1) %a
  ret ptr addrspace(1) %r
}
