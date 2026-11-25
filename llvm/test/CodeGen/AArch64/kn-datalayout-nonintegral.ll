; REQUIRES: aarch64-registered-target

; Address space 1 is the Kotlin object heap of the "kotlin-native" GC strategy. The
; 64-bit Mach-O AArch64 data layout declares it non-integral, so passes do not
; convert pointers of this address space to integers and back.

; The layout comes from the target machine when the module has none.
; RUN: opt -mtriple=arm64-apple-macosx -passes=verify -S < %s | FileCheck %s --check-prefix=MACHO
; RUN: opt -mtriple=arm64-apple-ios -passes=verify -S < %s | FileCheck %s --check-prefix=MACHO
; The 32-bit pointer Mach-O target and the other object formats are not changed.
; RUN: opt -mtriple=arm64_32-apple-watchos -passes=verify -S < %s | FileCheck %s --check-prefix=OTHER
; RUN: opt -mtriple=aarch64-unknown-linux-gnu -passes=verify -S < %s | FileCheck %s --check-prefix=OTHER

; MACHO: target datalayout = "{{.*}}-ni:1"
; OTHER: target datalayout = "
; OTHER-NOT: ni:1

define void @f() {
  ret void
}
