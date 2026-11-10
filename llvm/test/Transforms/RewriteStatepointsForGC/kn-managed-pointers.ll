; RUN: opt < %s -S -passes=rewrite-statepoints-for-gc | FileCheck %s

; The "kotlin-native" strategy manages exactly the pointers in address space 1
; (the Kotlin object heap). Pointers of every other address space are plain
; native memory: they are never put into gc-live and never relocated.

declare void @foo()

; CHECK-LABEL: @only_addrspace_1_is_managed(
define void @only_addrspace_1_is_managed(ptr addrspace(1) %managed, ptr %native, ptr addrspace(2) %other) gc "kotlin-native" {
entry:
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint{{.*}}[ "gc-live"(ptr addrspace(1) %managed) ]
; CHECK: call coldcc ptr addrspace(1) @llvm.experimental.gc.relocate.p1(
; CHECK-NOT: @llvm.experimental.gc.relocate.p0
; CHECK-NOT: @llvm.experimental.gc.relocate.p2
; CHECK: ret void
  call void @foo()
  store i8 0, ptr addrspace(1) %managed
  store i8 0, ptr %native
  store i8 0, ptr addrspace(2) %other
  ret void
}

; A function without managed pointers live across the call gets a statepoint
; without gc-live values.
; CHECK-LABEL: @no_managed_pointers(
define void @no_managed_pointers(ptr %native) gc "kotlin-native" {
entry:
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK-NOT: gc.relocate
; CHECK: ret void
  call void @foo()
  store i8 0, ptr %native
  ret void
}

; A managed pointer that is dead after the call is not live at the statepoint.
; CHECK-LABEL: @dead_after_call(
define void @dead_after_call(ptr addrspace(1) %managed) gc "kotlin-native" {
entry:
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK-NOT: gc-live
; CHECK-NOT: gc.relocate
; CHECK: ret void
  store i8 0, ptr addrspace(1) %managed
  call void @foo()
  ret void
}
