; REQUIRES: aarch64-registered-target

; A function of the "kotlin-native" strategy that has a stack map record stores the
; address of that record in its frame, 16 bytes below the frame pointer. The runtime
; reads the address from there (fp - 16) of every frame it walks and decodes only that
; record. The record exists exactly for the functions that have a statepoint, so only
; they store the address: a store without a record would not link.

; RUN: rm -rf %t && split-file %s %t
; RUN: llc -mtriple=arm64-apple-macosx -O2 %t/store.ll -o - | FileCheck %t/store.ll --check-prefix=MACHO
; RUN: llc -mtriple=arm64-apple-macosx -O0 %t/store.ll -o - | FileCheck %t/store.ll --check-prefix=MACHO
; RUN: llc -mtriple=aarch64-unknown-linux-gnu -O2 %t/store.ll -o - | FileCheck %t/store.ll --check-prefix=ELF
; RUN: llc -mtriple=arm64-apple-macosx -filetype=obj %t/store.ll -o %t/store.o
; RUN: llvm-objdump -r %t/store.o | FileCheck %t/store.ll --check-prefix=RELOC
; RUN: llc -mtriple=arm64-apple-macosx -O2 %t/no-store.ll -o - | FileCheck %t/no-store.ll
; RUN: llc -mtriple=arm64-apple-macosx -O2 -kn-lazy-stackmap=false %t/store.ll -o - | FileCheck %t/store.ll --check-prefix=EAGER
; RUN: llc -mtriple=arm64-apple-macosx -O2 -kn-lazy-stackmap=false %t/callee-saved.ll -o - | FileCheck %t/callee-saved.ll --check-prefix=EAGER-CSR
; RUN: llc -mtriple=arm64-apple-macosx -O2 %t/other-strategy.ll -o - | FileCheck %t/other-strategy.ll
; RUN: llc -mtriple=arm64-apple-macosx -O2 %t/callee-saved.ll -o - | FileCheck %t/callee-saved.ll

;--- store.ll
declare void @foo()
declare void @use(ptr addrspace(1))
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

define void @f(ptr addrspace(1) %a) gc "kotlin-native" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
  %a1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a1)
  ret void
}

; The store is right after the frame pointer is set, before the first call.
; MACHO-LABEL: _f:
; MACHO:       add x29, sp
; MACHO-NEXT:  adrp [[R:x[0-9]+]], .Lstackmap_delta_main_start._f@PAGE
; MACHO-NEXT:  add [[R]], [[R]], .Lstackmap_delta_main_start._f@PAGEOFF
; MACHO-NEXT:  stur [[R]], [x29, #-16]
; MACHO:       bl _foo

; The function refers to its record, so the linker may remove both together.
; MACHO:       .section __LLVM_STACKMAPS,__llvm_stackmaps{{$}}

; ELF-LABEL: f:
; ELF:       add x29, sp
; ELF:       adrp [[R:x[0-9]+]], .Lstackmap_delta_main_start.f
; ELF-NEXT:  add [[R]], [[R]], :lo12:.Lstackmap_delta_main_start.f
; ELF-NEXT:  stur [[R]], [x29, #-16]
; ELF:       bl foo

; The runtime that decodes the whole section at the start does not read the address:
; no store, no slot, but the record is the same.
; EAGER-LABEL: _f:
; EAGER-NOT:   adrp
; EAGER-NOT:   stur
; EAGER:       bl _foo
; EAGER:       .section __LLVM_STACKMAPS,__llvm_stackmaps,regular,no_dead_strip{{$}}
; EAGER:       .Lstackmap_delta_main_start._f:

; The address is a link-time constant, and the record knows its function by a distance
; to itself, so records of different objects can be concatenated by the linker.
; RELOC:      RELOCATION RECORDS FOR [__text]:
; RELOC-DAG:  ARM64_RELOC_PAGE21 .Lstackmap_delta_main_start._f
; RELOC-DAG:  ARM64_RELOC_PAGEOFF12 .Lstackmap_delta_main_start._f
; RELOC:      RELOCATION RECORDS FOR [__llvm_stackmaps]:
; RELOC-NEXT: OFFSET TYPE VALUE
; RELOC-NEXT: ARM64_RELOC_SUBTRACTOR .Lstackmap_delta_main_start._f
; RELOC-NEXT: ARM64_RELOC_UNSIGNED _f

;--- no-store.ll
declare void @foo()

; No statepoint: no record, so no store and no reference to the record.
; CHECK-LABEL: _plain_calls:
; CHECK-NOT:   stackmap
; CHECK:       bl _foo
define void @plain_calls() gc "kotlin-native" {
  call void @foo()
  ret void
}

; CHECK-LABEL: _leaf:
; CHECK-NOT:   stackmap
; CHECK:       ret
define i64 @leaf(i64 %x) gc "kotlin-native" {
  %y = add i64 %x, 1
  ret i64 %y
}


;--- other-strategy.ll
declare void @foo()
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)

; Another strategy does not use the address.
; CHECK-LABEL: _other_strategy:
; CHECK-NOT:   Lstackmap_delta_main_start
; CHECK:       bl _foo
define void @other_strategy() gc "statepoint-example" {
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0)
  ret void
}

;--- callee-saved.ll
declare i64 @bar(i64)
declare void @foo()
declare void @use(ptr addrspace(1), i64, i64, i64)
declare token @llvm.experimental.gc.statepoint.p0(i64, i32, ptr, i32, i32, ...)
declare ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token, i32, i32)

; The two words of the address slot are directly below the frame record, so the saved
; registers (x19 and the following) are below them: 16 bytes lower than without the slot.
; CHECK-LABEL: _csr:
; CHECK:       stur {{x[0-9]+}}, [x29, #-16]
; CHECK:       .cfi_offset w30, -8
; CHECK-NEXT:  .cfi_offset w29, -16
; CHECK-NEXT:  .cfi_offset w19, -40
; CHECK-NEXT:  .cfi_offset w20, -48
; EAGER-CSR-LABEL: _csr:
; EAGER-CSR-NOT:    stur
; EAGER-CSR:        .cfi_offset w30, -8
; EAGER-CSR-NEXT:   .cfi_offset w29, -16
; EAGER-CSR-NEXT:   .cfi_offset w19, -24
define void @csr(ptr addrspace(1) %a, i64 %x) gc "kotlin-native" {
  %p = call i64 @bar(i64 %x)
  %q = call i64 @bar(i64 %p)
  %t = call token (i64, i32, ptr, i32, i32, ...) @llvm.experimental.gc.statepoint.p0(i64 0, i32 0, ptr elementtype(void ()) @foo, i32 0, i32 0, i32 0, i32 0) [ "gc-live"(ptr addrspace(1) %a) ]
  %a1 = call ptr addrspace(1) @llvm.experimental.gc.relocate.p1(token %t, i32 0, i32 0)
  call void @use(ptr addrspace(1) %a1, i64 %x, i64 %p, i64 %q)
  ret void
}
