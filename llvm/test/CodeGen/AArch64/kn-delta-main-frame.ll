; REQUIRES: aarch64-registered-target

; The runtime finds the frames of Kotlin functions by following the chain of
; frame records ([saved fp, return address] at fp), and reads the GC roots at
; offsets from the frame pointer. So every function of the "kotlin-native"
; strategy must have a frame record and must keep sp - fp constant. The
; function attributes of the producer do not matter.

; RUN: rm -rf %t && split-file %s %t
; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O2 %t/frame-record.ll -o - | FileCheck %t/frame-record.ll
; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O0 %t/frame-record.ll -o - | FileCheck %t/frame-record.ll
; RUN: llc -mtriple=arm64-apple-macosx -O2 %t/frame-record.ll -o - | FileCheck %t/frame-record.ll
; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O2 %t/statepoint-frame.ll -o - | FileCheck %t/statepoint-frame.ll

; RUN: not --crash llc -mtriple=arm64-apple-macosx %t/dynamic-alloca.ll -o /dev/null 2>&1 | FileCheck %t/dynamic-alloca.ll
; RUN: llc -mtriple=arm64-apple-macosx %t/dynamic-alloca-other-strategy.ll -o /dev/null
; RUN: not --crash llc -mtriple=arm64-apple-macosx %t/overaligned-alloca.ll -o /dev/null 2>&1 | FileCheck %t/overaligned-alloca.ll
; RUN: llc -mtriple=arm64-apple-macosx %t/overaligned-alloca-other-strategy.ll -o /dev/null

; RUN: not --crash llc -mtriple=arm64-apple-macosx %t/gc-live-alloca.ll -o /dev/null 2>&1 | FileCheck %t/gc-live-alloca.ll

;--- frame-record.ll
; A leaf function of the strategy has a frame record. A leaf function of another
; strategy does not need one (control).
; CHECK-LABEL: leaf:
; CHECK:       stp x29, x30, [sp
; CHECK:       {{(mov|add)}} x29, sp
define i64 @leaf(i64 %x) gc "kotlin-native" {
  %y = add i64 %x, 1
  ret i64 %y
}

; A producer that asks for no frame pointer does not get that.
; CHECK-LABEL: leaf_no_frame_pointer:
; CHECK:       stp x29, x30, [sp
; CHECK:       {{(mov|add)}} x29, sp
define i64 @leaf_no_frame_pointer(i64 %x) #0 gc "kotlin-native" {
  %y = add i64 %x, 1
  ret i64 %y
}

; CHECK-LABEL: leaf_other_strategy:
; CHECK-NOT:   x29
; CHECK:       ret
define i64 @leaf_other_strategy(i64 %x) gc "statepoint-example" {
  %y = add i64 %x, 1
  ret i64 %y
}

attributes #0 = { "frame-pointer"="none" }

;--- statepoint-frame.ll
declare void @foo()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

; The frame pointer is set up before the first call, and the live GC pointer is
; kept below it, at an offset from x29 that the stack map describes.
; CHECK-LABEL: kn:
; CHECK:       stp x29, x30, [sp
; CHECK:       add x29, sp
; CHECK:       bl foo
define void @kn(ptr addrspace(1) %a) gc "kotlin-native" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
  %a1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a1)
  ret void
}

;--- dynamic-alloca.ll
; sp moves relative to fp after a dynamic alloca, so the fixed sp->fp conversion of
; the stack map offsets would be wrong. The compiler must refuse such a function.
define void @vla(i64 %n) gc "kotlin-native" {
  %buf = alloca i8, i64 %n
  store volatile i8 0, ptr %buf
  ret void
}
; CHECK: delta-main: function vla has var sized objects or stack realignment

;--- dynamic-alloca-other-strategy.ll
define void @vla(i64 %n) gc "statepoint-example" {
  %buf = alloca i8, i64 %n
  store volatile i8 0, ptr %buf
  ret void
}

;--- overaligned-alloca.ll
; Stack realignment moves sp relative to fp in the same way.
define void @aligned() gc "kotlin-native" {
  %buf = alloca i8, i32 64, align 64
  store volatile i8 0, ptr %buf
  ret void
}
; CHECK: delta-main: function aligned has var sized objects or stack realignment

;--- overaligned-alloca-other-strategy.ll
define void @aligned() gc "statepoint-example" {
  %buf = alloca i8, i32 64, align 64
  store volatile i8 0, ptr %buf
  ret void
}

;--- gc-live-alloca.ll
declare void @foo()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

; Only values in stack slots are described by the format. A stack object that is
; itself a GC root (an alloca in gc-live) has no encoding: the compiler stops
; instead of writing a stack map that misses the root.
define void @stack_object() gc "kotlin-native" {
  %obj = alloca i64, addrspace(1)
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %obj) ]
  %o1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  call void @use(ptr addrspace(1) %o1)
  ret void
}
; CHECK: LLVM ERROR: delta-main:
