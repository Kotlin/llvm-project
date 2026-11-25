; llvm.experimental.kn.gc.relocate is the form of gc.relocate for the exceptional
; path of an invoke statepoint whose landingpad has a real (non-token) type. It
; gets the i32 selector extracted from the landingpad instead of a token.
; The verifier checks it with the same rules as llvm.experimental.gc.relocate.

; RUN: rm -rf %t && split-file %s %t
; RUN: opt -passes=verify -disable-output %t/valid.ll
; RUN: not opt -passes=verify -disable-output %t/landingpad-without-statepoint.ll 2>&1 | FileCheck %t/landingpad-without-statepoint.ll
; RUN: not opt -passes=verify -disable-output %t/index-out-of-bounds.ll 2>&1 | FileCheck %t/index-out-of-bounds.ll
; RUN: not opt -passes=verify -disable-output %t/not-managed-pointer.ll 2>&1 | FileCheck %t/not-managed-pointer.ll

;--- valid.ll
declare void @foo()
declare i32 @personality()
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)
declare ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32, i32, i32)

define ptr addrspace(1) @valid(ptr addrspace(1) %a, ptr addrspace(1) %b) gc "kotlin-native" personality ptr @personality {
entry:
  %tok = invoke token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a, ptr addrspace(1) %b) ]
          to label %normal unwind label %lpad

normal:
  %a.n = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %tok, i32 0, i32 0)
  ret ptr addrspace(1) %a.n

lpad:
  %lp = landingpad { ptr, i32 } cleanup
  %sel = extractvalue { ptr, i32 } %lp, 1
  %a.l = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %sel, i32 0, i32 0)
  %b.l = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %sel, i32 1, i32 1)
  ret ptr addrspace(1) %b.l
}

;--- landingpad-without-statepoint.ll
; The landingpad is reached from an ordinary invoke, not from an invoke
; statepoint.
declare void @foo()
declare i32 @personality()
declare ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32, i32, i32)

define ptr addrspace(1) @f() gc "kotlin-native" personality ptr @personality {
entry:
  invoke void @foo() to label %normal unwind label %lpad

normal:
  ret ptr addrspace(1) null

lpad:
  %lp = landingpad { ptr, i32 } cleanup
  %sel = extractvalue { ptr, i32 } %lp, 1
  %r = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %sel, i32 0, i32 0)
  ret ptr addrspace(1) %r
}
; CHECK: gc relocate should be linked to a statepoint

;--- index-out-of-bounds.ll
; The statepoint has one gc-live value, the relocate asks for the third.
declare void @foo()
declare i32 @personality()
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32, i32, i32)

define ptr addrspace(1) @f(ptr addrspace(1) %a) gc "kotlin-native" personality ptr @personality {
entry:
  %tok = invoke token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
          to label %normal unwind label %lpad

normal:
  ret ptr addrspace(1) null

lpad:
  %lp = landingpad { ptr, i32 } cleanup
  %sel = extractvalue { ptr, i32 } %lp, 1
  %r = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %sel, i32 2, i32 2)
  ret ptr addrspace(1) %r
}
; CHECK: gc.relocate: statepoint base index out of bounds

;--- not-managed-pointer.ll
; Under "kotlin-native" only address space 1 is managed: a relocate of an
; address space 0 pointer is rejected.
declare void @foo()
declare i32 @personality()
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr @llvm.experimental.kn.gc.relocate.p0(i32, i32, i32)

define ptr @f(ptr %a) gc "kotlin-native" personality ptr @personality {
entry:
  %tok = invoke token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr %a) ]
          to label %normal unwind label %lpad

normal:
  ret ptr null

lpad:
  %lp = landingpad { ptr, i32 } cleanup
  %sel = extractvalue { ptr, i32 } %lp, 1
  %r = call ptr @llvm.experimental.kn.gc.relocate.p0(i32 %sel, i32 0, i32 0)
  ret ptr %r
}
; CHECK: gc.relocate: must return gc pointer
