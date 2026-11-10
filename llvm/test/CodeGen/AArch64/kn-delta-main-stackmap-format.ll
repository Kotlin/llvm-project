; REQUIRES: aarch64-registered-target

; The "kotlin-native" GC strategy emits its stack maps in the delta-main format,
; which the Kotlin/Native runtime decodes. These tests pin the wire format, the
; contract between the compiler and the runtime, as the assembler directives
; that produce it (the emitter writes it with .byte/.long/.uleb128 directives):
;
;   per function, one record that starts at the label
;   .Lstackmap_delta_main_start.<function>; a function stores the address of this
;   label in its frame, and the runtime finds the record from it:
;     start     .long   function address - address of the record
;     size      .long   size of the function in bytes
;     base      SLEB128 offset of the first slot from the frame pointer (<= 0)
;     size      ULEB128 frame size
;     live      ULEB128 bit vector: slot i is the slot at (base - 8 * i)
;     calls     ULEB128 count, then (call return address offset, delta index)
;     deltas    one ULEB128 bit vector per distinct delta; the live set of a
;               call is (live ^ delta)
;
; The runtime test DeltaMainStackMapTest uses the same byte sequences.

; RUN: rm -rf %t && split-file %s %t
; RUN: llc -O0 %t/one-value.ll -o - | FileCheck %t/one-value.ll
; RUN: llc -O0 %t/no-live-values.ll -o - | FileCheck %t/no-live-values.ll
; RUN: llc -O0 %t/eight-values.ll -o - | FileCheck %t/eight-values.ll
; RUN: llc -O0 %t/shrinking-live-set.ll -o - | FileCheck %t/shrinking-live-set.ll
; RUN: llc -O0 %t/same-live-set.ll -o - | FileCheck %t/same-live-set.ll
; RUN: llc -O0 %t/two-functions.ll -o - | FileCheck %t/two-functions.ll
; RUN: llc -O0 %t/no-statepoints.ll -o - | FileCheck %t/no-statepoints.ll
; RUN: llc -O0 %t/other-strategy.ll -o - | FileCheck %t/other-strategy.ll
; RUN: llc -O0 %t/invoke.ll -o - | FileCheck %t/invoke.ll
; RUN: llc -O2 %t/one-value.ll -o - | FileCheck %t/one-value.ll

;--- one-value.ll
target triple = "arm64-apple-macosx"

declare void @foo()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

define void @one(ptr addrspace(1) %a) gc "kotlin-native" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
  %a1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a1)
  ret void
}

; The section is named like the LLVM stack maps section.
; CHECK:      .section __LLVM_STACKMAPS,__llvm_stackmaps
; CHECK-NEXT: .Lstackmap_delta_main_start._one:
; CHECK-NEXT: .long _one-.Lstackmap_delta_main_start._one
; CHECK-NEXT: .long Lkn_func_end{{[0-9]+}}-_one
; The first slot is below the frame pointer, so the SLEB128 base is negative:
; its single byte has the sign bit (bit 6) set.
; CHECK-NEXT: .byte {{6[4-9]|[7-9][0-9]|1[0-2][0-9]}}
; frame size
; CHECK-NEXT: .byte {{[0-9]+}}
; base live set: one slot
; CHECK-NEXT: .byte 1
; one call
; CHECK-NEXT: .byte 1
; CHECK-NEXT: .uleb128 Ltmp{{[0-9]+}}-_one
; CHECK-NEXT: .byte 0
; its delta is empty: the live set of the call is the base set
; CHECK-NEXT: .byte 0

;--- no-live-values.ll
target triple = "arm64-apple-macosx"

declare void @foo()
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)

define void @nolive() gc "kotlin-native" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0)
  ret void
}

; A call that has no live GC pointers is still recorded, with an empty live set.
; CHECK:      .Lstackmap_delta_main_start._nolive:
; CHECK-NEXT: .long _nolive-.Lstackmap_delta_main_start._nolive
; CHECK-NEXT: .long Lkn_func_end{{[0-9]+}}-_nolive
; CHECK-NEXT: .byte {{[0-9]+}}
; CHECK-NEXT: .byte {{[0-9]+}}
; empty base live set
; CHECK-NEXT: .byte 0
; one call
; CHECK-NEXT: .byte 1
; CHECK-NEXT: .uleb128 Ltmp{{[0-9]+}}-_nolive
; CHECK-NEXT: .byte 0
; CHECK-NEXT: .byte 0

;--- eight-values.ll
target triple = "arm64-apple-macosx"

declare void @foo()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

define void @eight(ptr addrspace(1) %a0, ptr addrspace(1) %a1, ptr addrspace(1) %a2, ptr addrspace(1) %a3, ptr addrspace(1) %a4, ptr addrspace(1) %a5, ptr addrspace(1) %a6, ptr addrspace(1) %a7) gc "kotlin-native" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a0, ptr addrspace(1) %a1, ptr addrspace(1) %a2, ptr addrspace(1) %a3, ptr addrspace(1) %a4, ptr addrspace(1) %a5, ptr addrspace(1) %a6, ptr addrspace(1) %a7) ]
  %r0 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  %r1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 1, i32 1)
  %r2 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 2, i32 2)
  %r3 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 3, i32 3)
  %r4 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 4, i32 4)
  %r5 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 5, i32 5)
  %r6 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 6, i32 6)
  %r7 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 7, i32 7)
  call void @use(ptr addrspace(1) %r0)
  call void @use(ptr addrspace(1) %r1)
  call void @use(ptr addrspace(1) %r2)
  call void @use(ptr addrspace(1) %r3)
  call void @use(ptr addrspace(1) %r4)
  call void @use(ptr addrspace(1) %r5)
  call void @use(ptr addrspace(1) %r6)
  call void @use(ptr addrspace(1) %r7)
  ret void
}

; The live set is a bit vector in ULEB128: 7 bits per byte. Eight slots need two
; bytes: seven set bits with the continuation flag (0xFF = "\377"), then one bit.
; CHECK:      .long _eight-.Lstackmap_delta_main_start._eight
; CHECK-NEXT: .long Lkn_func_end{{[0-9]+}}-_eight
; (the base and the frame size come first, each as one or more bytes)
; CHECK:      .ascii "\377\001"

;--- shrinking-live-set.ll
target triple = "arm64-apple-macosx"

declare void @foo()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

; Two values are live at the first call, one at the second. Two calls, two deltas.
define void @shrink(ptr addrspace(1) %a, ptr addrspace(1) %b) gc "kotlin-native" {
  %t1 = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a, ptr addrspace(1) %b) ]
  %a1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t1, i32 0, i32 0)
  %b1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t1, i32 1, i32 1)
  call void @use(ptr addrspace(1) %b1)
  %t2 = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a1) ]
  %a2 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t2, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a2)
  ret void
}

; CHECK:      .Lstackmap_delta_main_start._shrink:
; CHECK-NEXT: .long _shrink-.Lstackmap_delta_main_start._shrink
; CHECK-NEXT: .long Lkn_func_end{{[0-9]+}}-_shrink
; CHECK-NEXT: .byte {{[0-9]+}}
; CHECK-NEXT: .byte {{[0-9]+}}
; the base set is the live set of the first call: two slots
; CHECK-NEXT: .byte 3
; two calls
; CHECK-NEXT: .byte 2
; CHECK-NEXT: .uleb128 Ltmp{{[0-9]+}}-_shrink
; CHECK-NEXT: .byte 0
; CHECK-NEXT: .uleb128 Ltmp{{[0-9]+}}-_shrink
; CHECK-NEXT: .byte 1
; two deltas: empty for the first call; for the second call one slot is dropped
; (which one depends on the slot assignment: bit 0 or bit 1)
; CHECK-NEXT: .byte 0
; CHECK-NEXT: .byte {{1|2}}

;--- same-live-set.ll
target triple = "arm64-apple-macosx"

declare void @foo()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

; The same set is live at both calls: both calls use the same delta, stored once.
define void @same(ptr addrspace(1) %a) gc "kotlin-native" {
  %t1 = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
  %a1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t1, i32 0, i32 0)
  %t2 = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a1) ]
  %a2 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t2, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a2)
  ret void
}

; CHECK:      .Lstackmap_delta_main_start._same:
; CHECK-NEXT: .long _same-.Lstackmap_delta_main_start._same
; CHECK-NEXT: .long Lkn_func_end{{[0-9]+}}-_same
; CHECK-NEXT: .byte {{[0-9]+}}
; CHECK-NEXT: .byte {{[0-9]+}}
; CHECK-NEXT: .byte 1
; two calls, both with delta 0
; CHECK-NEXT: .byte 2
; CHECK-NEXT: .uleb128 Ltmp{{[0-9]+}}-_same
; CHECK-NEXT: .byte 0
; CHECK-NEXT: .uleb128 Ltmp{{[0-9]+}}-_same
; CHECK-NEXT: .byte 0
; exactly one delta follows, then the section ends
; CHECK-NEXT: .byte 0
; CHECK-NOT:  .byte
; CHECK:      .subsections_via_symbols

;--- two-functions.ll
target triple = "arm64-apple-macosx"

declare void @foo()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

define void @first(ptr addrspace(1) %a) gc "kotlin-native" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
  %a1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a1)
  ret void
}

define void @second(ptr addrspace(1) %a) gc "kotlin-native" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
  %a1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a1)
  ret void
}

; two functions, each record has its own start label and is relative to it
; CHECK:      .Lstackmap_delta_main_start._first:
; CHECK-NEXT: .long _first-.Lstackmap_delta_main_start._first
; CHECK:      .Lstackmap_delta_main_start._second:
; CHECK-NEXT: .long _second-.Lstackmap_delta_main_start._second

;--- no-statepoints.ll
target triple = "arm64-apple-macosx"

declare void @foo()

; A function without statepoints has nothing to describe: no section at all.
define void @nostate() gc "kotlin-native" {
  call void @foo()
  ret void
}

; CHECK-NOT: __LLVM_STACKMAPS
; CHECK-NOT: Lstackmap_delta_main_start
; CHECK:     _nostate:

;--- other-strategy.ll
target triple = "arm64-apple-macosx"

declare void @foo()
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)

; A statepoint of another strategy keeps the standard LLVM stack map format
; (version 3), not the delta-main one.
define void @other() gc "statepoint-example" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0)
  ret void
}

; CHECK:      __LLVM_StackMaps:
; CHECK-NEXT: .byte 3
; CHECK-NOT:  Lstackmap_delta_main_start

;--- invoke.ll
target triple = "arm64-apple-macosx"

declare void @foo()
declare i32 @personality()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)
declare ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32, i32, i32)

; A call that can throw: the exceptional edge uses kn.gc.relocate. The function
; has one stack map record that covers the call.
define void @thrower(ptr addrspace(1) %a) gc "kotlin-native" personality ptr @personality {
entry:
  %t = invoke token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
          to label %normal unwind label %lpad

normal:
  %a.n = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a.n)
  ret void

lpad:
  %lp = landingpad { ptr, i32 } cleanup
  %sel = extractvalue { ptr, i32 } %lp, 1
  %a.l = call ptr addrspace(1) @llvm.experimental.kn.gc.relocate.p1(i32 %sel, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a.l)
  resume { ptr, i32 } %lp
}

; CHECK:      .Lstackmap_delta_main_start._thrower:
; CHECK-NEXT: .long _thrower-.Lstackmap_delta_main_start._thrower
