; Calls from a `gc "kotlin-native"` function to a fixed set of Kotlin/Native
; runtime helpers are treated as gc-leaf: they are not wrapped into a
; statepoint, even though the declarations carry no "gc-leaf-function"
; attribute. The helpers are matched by mangled name AND by signature, and
; only for callers that use the "kotlin-native" GC strategy.

; RUN: rm -rf %t && split-file %s %t
; RUN: opt -S -passes=rewrite-statepoints-for-gc %t/registered.ll | FileCheck %t/registered.ll
; RUN: opt -S -passes=rewrite-statepoints-for-gc %t/other-strategy.ll | FileCheck %t/other-strategy.ll
; RUN: opt -S -passes=rewrite-statepoints-for-gc %t/wrong-signature.ll | FileCheck %t/wrong-signature.ll
; RUN: opt -S -passes=rewrite-statepoints-for-gc %t/nobuiltin.ll | FileCheck %t/nobuiltin.ll

;--- registered.ll
; Every registered helper, declared with the signature the runtime really has
; (the C++ `bool` is passed as i1, `std_support::span<char>` as [2 x i64]).
declare void @_ZN5konan13consolePrintfEPKcz(ptr, ...)
declare void @_ZN5konan13consoleErrorfEPKcz(ptr, ...)
declare void @_ZN6kotlin12FormatToSpanENS_11std_support4spanIcLm18446744073709551615EEEPKcz([2 x i64], ptr, ...)
declare void @_ZN6kotlin7logging3LogENS0_5LevelESt16initializer_listINS0_3TagEEPKcz(i32, [2 x i64], ptr, ...)
declare void @_ZN6kotlin8internal22RuntimeAssertFailedLogEbPKcS2_z(i1, ptr, ptr, ...)
declare void @_ZN6kotlin8internal24RuntimeAssertFailedPanicEbPKcS2_z(i1, ptr, ptr, ...)
declare i32 @_ZN6kotlin18snprintf_with_addrENS_11std_support4spanIcLm18446744073709551615EEEmPKvbPKcz([2 x i64], i64, ptr, i1, ptr, ...)
declare void @_ZN6kotlin29throwReadingRandomBytesFailedEPKcz(ptr, ...)
declare void @not_registered()

; The managed pointer %obj is live across every call. A real statepoint would
; put it into gc-live and relocate it; a gc-leaf call leaves it untouched.

; CHECK-LABEL: @console_printf(
; CHECK-NOT: gc.statepoint
; CHECK: ret ptr addrspace(1) %obj
define ptr addrspace(1) @console_printf(ptr addrspace(1) %obj, ptr %s) gc "kotlin-native" {
  call void (ptr, ...) @_ZN5konan13consolePrintfEPKcz(ptr %s)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @console_errorf(
; CHECK-NOT: gc.statepoint
; CHECK: ret ptr addrspace(1) %obj
define ptr addrspace(1) @console_errorf(ptr addrspace(1) %obj, ptr %s) gc "kotlin-native" {
  call void (ptr, ...) @_ZN5konan13consoleErrorfEPKcz(ptr %s)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @format_to_span(
; CHECK-NOT: gc.statepoint
; CHECK: ret ptr addrspace(1) %obj
define ptr addrspace(1) @format_to_span(ptr addrspace(1) %obj, [2 x i64] %span, ptr %s) gc "kotlin-native" {
  call void ([2 x i64], ptr, ...) @_ZN6kotlin12FormatToSpanENS_11std_support4spanIcLm18446744073709551615EEEPKcz([2 x i64] %span, ptr %s)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @logging_log(
; CHECK-NOT: gc.statepoint
; CHECK: ret ptr addrspace(1) %obj
define ptr addrspace(1) @logging_log(ptr addrspace(1) %obj, [2 x i64] %tags, ptr %s) gc "kotlin-native" {
  call void (i32, [2 x i64], ptr, ...) @_ZN6kotlin7logging3LogENS0_5LevelESt16initializer_listINS0_3TagEEPKcz(i32 1, [2 x i64] %tags, ptr %s)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @assert_failed_log(
; CHECK-NOT: gc.statepoint
; CHECK: ret ptr addrspace(1) %obj
define ptr addrspace(1) @assert_failed_log(ptr addrspace(1) %obj, ptr %s) gc "kotlin-native" {
  call void (i1, ptr, ptr, ...) @_ZN6kotlin8internal22RuntimeAssertFailedLogEbPKcS2_z(i1 true, ptr %s, ptr %s)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @assert_failed_panic(
; CHECK-NOT: gc.statepoint
; CHECK: ret ptr addrspace(1) %obj
define ptr addrspace(1) @assert_failed_panic(ptr addrspace(1) %obj, ptr %s) gc "kotlin-native" {
  call void (i1, ptr, ptr, ...) @_ZN6kotlin8internal24RuntimeAssertFailedPanicEbPKcS2_z(i1 true, ptr %s, ptr %s)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @snprintf_with_addr(
; CHECK-NOT: gc.statepoint
; CHECK: ret ptr addrspace(1) %obj
define ptr addrspace(1) @snprintf_with_addr(ptr addrspace(1) %obj, [2 x i64] %buf, ptr %s) gc "kotlin-native" {
  %n = call i32 ([2 x i64], i64, ptr, i1, ptr, ...) @_ZN6kotlin18snprintf_with_addrENS_11std_support4spanIcLm18446744073709551615EEEmPKvbPKcz([2 x i64] %buf, i64 16, ptr %s, i1 false, ptr %s)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @throw_reading_random_bytes_failed(
; CHECK-NOT: gc.statepoint
; CHECK: ret ptr addrspace(1) %obj
define ptr addrspace(1) @throw_reading_random_bytes_failed(ptr addrspace(1) %obj, ptr %s) gc "kotlin-native" {
  call void (ptr, ...) @_ZN6kotlin29throwReadingRandomBytesFailedEPKcz(ptr %s)
  ret ptr addrspace(1) %obj
}

; Control: an ordinary call is a statepoint, the managed pointer is relocated.
; CHECK-LABEL: @control_not_registered(
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK: [[R:%.*]] = call coldcc ptr addrspace(1) @llvm.experimental.gc.relocate
; CHECK: ret ptr addrspace(1) [[R]]
define ptr addrspace(1) @control_not_registered(ptr addrspace(1) %obj) gc "kotlin-native" {
  call void @not_registered()
  ret ptr addrspace(1) %obj
}

;--- other-strategy.ll
; The registry is consulted only for callers of the "kotlin-native" strategy.
declare void @_ZN5konan13consolePrintfEPKcz(ptr, ...)

; CHECK-LABEL: @other_strategy(
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK: ret ptr addrspace(1)
define ptr addrspace(1) @other_strategy(ptr addrspace(1) %obj, ptr %s) gc "statepoint-example" {
  call void (ptr, ...) @_ZN5konan13consolePrintfEPKcz(ptr %s)
  ret ptr addrspace(1) %obj
}

;--- wrong-signature.ll
; A function with a registered name but another signature is not a leaf.
; Same name, but not variadic:
declare void @_ZN5konan13consolePrintfEPKcz(ptr)
; First parameter is not a pointer:
declare void @_ZN5konan13consoleErrorfEPKcz(i32, ...)
; The first parameter must be a bool (i1), here it is a pointer:
declare void @_ZN6kotlin8internal22RuntimeAssertFailedLogEbPKcS2_z(ptr, ptr, ptr, ...)

; CHECK-LABEL: @not_variadic(
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK: ret ptr addrspace(1)
define ptr addrspace(1) @not_variadic(ptr addrspace(1) %obj, ptr %s) gc "kotlin-native" {
  call void @_ZN5konan13consolePrintfEPKcz(ptr %s)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @first_parameter_not_pointer(
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK: ret ptr addrspace(1)
define ptr addrspace(1) @first_parameter_not_pointer(ptr addrspace(1) %obj) gc "kotlin-native" {
  call void (i32, ...) @_ZN5konan13consoleErrorfEPKcz(i32 0)
  ret ptr addrspace(1) %obj
}

; CHECK-LABEL: @bool_parameter_is_a_pointer(
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK: ret ptr addrspace(1)
define ptr addrspace(1) @bool_parameter_is_a_pointer(ptr addrspace(1) %obj, ptr %s) gc "kotlin-native" {
  call void (ptr, ptr, ptr, ...) @_ZN6kotlin8internal22RuntimeAssertFailedLogEbPKcS2_z(ptr %s, ptr %s, ptr %s)
  ret ptr addrspace(1) %obj
}

;--- nobuiltin.ll
; A call site marked `nobuiltin` is never recognized as a library call.
declare void @_ZN5konan13consolePrintfEPKcz(ptr, ...)

; CHECK-LABEL: @nobuiltin_call(
; CHECK: call token {{.*}} @llvm.experimental.gc.statepoint
; CHECK: ret ptr addrspace(1)
define ptr addrspace(1) @nobuiltin_call(ptr addrspace(1) %obj, ptr %s) gc "kotlin-native" {
  call void (ptr, ...) @_ZN5konan13consolePrintfEPKcz(ptr %s) #0
  ret ptr addrspace(1) %obj
}

attributes #0 = { nobuiltin }
