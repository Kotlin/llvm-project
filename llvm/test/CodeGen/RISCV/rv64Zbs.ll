; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=riscv64 -verify-machineinstrs < %s \
; RUN:   | FileCheck %s -check-prefix=RV64I
; RUN: llc -mtriple=riscv64 -mattr=+experimental-b -verify-machineinstrs < %s \
; RUN:   | FileCheck %s -check-prefix=RV64IB
; RUN: llc -mtriple=riscv64 -mattr=+experimental-zbs -verify-machineinstrs < %s \
; RUN:   | FileCheck %s -check-prefix=RV64IBS

define signext i32 @sbclr_i32(i32 signext %a, i32 signext %b) nounwind {
; RV64I-LABEL: sbclr_i32:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    not a1, a1
; RV64I-NEXT:    and a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbclr_i32:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbclrw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbclr_i32:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbclrw a0, a0, a1
; RV64IBS-NEXT:    ret
  %and = and i32 %b, 31
  %shl = shl nuw i32 1, %and
  %neg = xor i32 %shl, -1
  %and1 = and i32 %neg, %a
  ret i32 %and1
}

define signext i32 @sbclr_i32_no_mask(i32 signext %a, i32 signext %b) nounwind {
; RV64I-LABEL: sbclr_i32_no_mask:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    not a1, a1
; RV64I-NEXT:    and a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbclr_i32_no_mask:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbclrw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbclr_i32_no_mask:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbclrw a0, a0, a1
; RV64IBS-NEXT:    ret
  %shl = shl i32 1, %b
  %neg = xor i32 %shl, -1
  %and1 = and i32 %neg, %a
  ret i32 %and1
}

define signext i32 @sbclr_i32_load(i32* %p, i32 signext %b) nounwind {
; RV64I-LABEL: sbclr_i32_load:
; RV64I:       # %bb.0:
; RV64I-NEXT:    lw a0, 0(a0)
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    not a1, a1
; RV64I-NEXT:    and a0, a1, a0
; RV64I-NEXT:    sext.w a0, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbclr_i32_load:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    lw a0, 0(a0)
; RV64IB-NEXT:    sbclrw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbclr_i32_load:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    lw a0, 0(a0)
; RV64IBS-NEXT:    sbclrw a0, a0, a1
; RV64IBS-NEXT:    ret
  %a = load i32, i32* %p
  %shl = shl i32 1, %b
  %neg = xor i32 %shl, -1
  %and1 = and i32 %neg, %a
  ret i32 %and1
}

define i64 @sbclr_i64(i64 %a, i64 %b) nounwind {
; RV64I-LABEL: sbclr_i64:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sll a1, a2, a1
; RV64I-NEXT:    not a1, a1
; RV64I-NEXT:    and a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbclr_i64:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbclr a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbclr_i64:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbclr a0, a0, a1
; RV64IBS-NEXT:    ret
  %and = and i64 %b, 63
  %shl = shl nuw i64 1, %and
  %neg = xor i64 %shl, -1
  %and1 = and i64 %neg, %a
  ret i64 %and1
}

define i64 @sbclr_i64_no_mask(i64 %a, i64 %b) nounwind {
; RV64I-LABEL: sbclr_i64_no_mask:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sll a1, a2, a1
; RV64I-NEXT:    not a1, a1
; RV64I-NEXT:    and a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbclr_i64_no_mask:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbclr a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbclr_i64_no_mask:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbclr a0, a0, a1
; RV64IBS-NEXT:    ret
  %shl = shl i64 1, %b
  %neg = xor i64 %shl, -1
  %and1 = and i64 %neg, %a
  ret i64 %and1
}

define signext i32 @sbset_i32(i32 signext %a, i32 signext %b) nounwind {
; RV64I-LABEL: sbset_i32:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    or a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbset_i32:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbsetw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbset_i32:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbsetw a0, a0, a1
; RV64IBS-NEXT:    ret
  %and = and i32 %b, 31
  %shl = shl nuw i32 1, %and
  %or = or i32 %shl, %a
  ret i32 %or
}

define signext i32 @sbset_i32_no_mask(i32 signext %a, i32 signext %b) nounwind {
; RV64I-LABEL: sbset_i32_no_mask:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    or a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbset_i32_no_mask:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbsetw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbset_i32_no_mask:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbsetw a0, a0, a1
; RV64IBS-NEXT:    ret
  %shl = shl i32 1, %b
  %or = or i32 %shl, %a
  ret i32 %or
}

define signext i32 @sbset_i32_load(i32* %p, i32 signext %b) nounwind {
; RV64I-LABEL: sbset_i32_load:
; RV64I:       # %bb.0:
; RV64I-NEXT:    lw a0, 0(a0)
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    or a0, a1, a0
; RV64I-NEXT:    sext.w a0, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbset_i32_load:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    lw a0, 0(a0)
; RV64IB-NEXT:    sbsetw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbset_i32_load:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    lw a0, 0(a0)
; RV64IBS-NEXT:    sbsetw a0, a0, a1
; RV64IBS-NEXT:    ret
  %a = load i32, i32* %p
  %shl = shl i32 1, %b
  %or = or i32 %shl, %a
  ret i32 %or
}

define i64 @sbset_i64(i64 %a, i64 %b) nounwind {
; RV64I-LABEL: sbset_i64:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sll a1, a2, a1
; RV64I-NEXT:    or a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbset_i64:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbset a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbset_i64:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbset a0, a0, a1
; RV64IBS-NEXT:    ret
  %conv = and i64 %b, 63
  %shl = shl nuw i64 1, %conv
  %or = or i64 %shl, %a
  ret i64 %or
}

define i64 @sbset_i64_no_mask(i64 %a, i64 %b) nounwind {
; RV64I-LABEL: sbset_i64_no_mask:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sll a1, a2, a1
; RV64I-NEXT:    or a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbset_i64_no_mask:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbset a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbset_i64_no_mask:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbset a0, a0, a1
; RV64IBS-NEXT:    ret
  %shl = shl i64 1, %b
  %or = or i64 %shl, %a
  ret i64 %or
}

define signext i32 @sbinv_i32(i32 signext %a, i32 signext %b) nounwind {
; RV64I-LABEL: sbinv_i32:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    xor a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbinv_i32:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbinvw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbinv_i32:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbinvw a0, a0, a1
; RV64IBS-NEXT:    ret
  %and = and i32 %b, 31
  %shl = shl nuw i32 1, %and
  %xor = xor i32 %shl, %a
  ret i32 %xor
}

define signext i32 @sbinv_i32_no_mask(i32 signext %a, i32 signext %b) nounwind {
; RV64I-LABEL: sbinv_i32_no_mask:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    xor a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbinv_i32_no_mask:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbinvw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbinv_i32_no_mask:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbinvw a0, a0, a1
; RV64IBS-NEXT:    ret
  %shl = shl i32 1, %b
  %xor = xor i32 %shl, %a
  ret i32 %xor
}

define signext i32 @sbinv_i32_load(i32* %p, i32 signext %b) nounwind {
; RV64I-LABEL: sbinv_i32_load:
; RV64I:       # %bb.0:
; RV64I-NEXT:    lw a0, 0(a0)
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sllw a1, a2, a1
; RV64I-NEXT:    xor a0, a1, a0
; RV64I-NEXT:    sext.w a0, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbinv_i32_load:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    lw a0, 0(a0)
; RV64IB-NEXT:    sbinvw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbinv_i32_load:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    lw a0, 0(a0)
; RV64IBS-NEXT:    sbinvw a0, a0, a1
; RV64IBS-NEXT:    ret
  %a = load i32, i32* %p
  %shl = shl i32 1, %b
  %xor = xor i32 %shl, %a
  ret i32 %xor
}

define i64 @sbinv_i64(i64 %a, i64 %b) nounwind {
; RV64I-LABEL: sbinv_i64:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sll a1, a2, a1
; RV64I-NEXT:    xor a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbinv_i64:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbinv a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbinv_i64:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbinv a0, a0, a1
; RV64IBS-NEXT:    ret
  %conv = and i64 %b, 63
  %shl = shl nuw i64 1, %conv
  %xor = xor i64 %shl, %a
  ret i64 %xor
}

define i64 @sbinv_i64_no_mask(i64 %a, i64 %b) nounwind {
; RV64I-LABEL: sbinv_i64_no_mask:
; RV64I:       # %bb.0:
; RV64I-NEXT:    addi a2, zero, 1
; RV64I-NEXT:    sll a1, a2, a1
; RV64I-NEXT:    xor a0, a1, a0
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbinv_i64_no_mask:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbinv a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbinv_i64_no_mask:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbinv a0, a0, a1
; RV64IBS-NEXT:    ret
  %shl = shl nuw i64 1, %b
  %xor = xor i64 %shl, %a
  ret i64 %xor
}

define signext i32 @sbext_i32(i32 signext %a, i32 signext %b) nounwind {
; RV64I-LABEL: sbext_i32:
; RV64I:       # %bb.0:
; RV64I-NEXT:    srlw a0, a0, a1
; RV64I-NEXT:    andi a0, a0, 1
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbext_i32:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbextw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbext_i32:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbextw a0, a0, a1
; RV64IBS-NEXT:    ret
  %and = and i32 %b, 31
  %shr = lshr i32 %a, %and
  %and1 = and i32 %shr, 1
  ret i32 %and1
}

define signext i32 @sbext_i32_no_mask(i32 signext %a, i32 signext %b) nounwind {
; RV64I-LABEL: sbext_i32_no_mask:
; RV64I:       # %bb.0:
; RV64I-NEXT:    srlw a0, a0, a1
; RV64I-NEXT:    andi a0, a0, 1
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbext_i32_no_mask:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbextw a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbext_i32_no_mask:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbextw a0, a0, a1
; RV64IBS-NEXT:    ret
  %shr = lshr i32 %a, %b
  %and1 = and i32 %shr, 1
  ret i32 %and1
}

define i64 @sbext_i64(i64 %a, i64 %b) nounwind {
; RV64I-LABEL: sbext_i64:
; RV64I:       # %bb.0:
; RV64I-NEXT:    srl a0, a0, a1
; RV64I-NEXT:    andi a0, a0, 1
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbext_i64:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbext a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbext_i64:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbext a0, a0, a1
; RV64IBS-NEXT:    ret
  %conv = and i64 %b, 63
  %shr = lshr i64 %a, %conv
  %and1 = and i64 %shr, 1
  ret i64 %and1
}

define i64 @sbext_i64_no_mask(i64 %a, i64 %b) nounwind {
; RV64I-LABEL: sbext_i64_no_mask:
; RV64I:       # %bb.0:
; RV64I-NEXT:    srl a0, a0, a1
; RV64I-NEXT:    andi a0, a0, 1
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbext_i64_no_mask:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbext a0, a0, a1
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbext_i64_no_mask:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbext a0, a0, a1
; RV64IBS-NEXT:    ret
  %shr = lshr i64 %a, %b
  %and1 = and i64 %shr, 1
  ret i64 %and1
}

define signext i32 @sbexti_i32(i32 signext %a) nounwind {
; RV64I-LABEL: sbexti_i32:
; RV64I:       # %bb.0:
; RV64I-NEXT:    srli a0, a0, 5
; RV64I-NEXT:    andi a0, a0, 1
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbexti_i32:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbexti a0, a0, 5
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbexti_i32:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbexti a0, a0, 5
; RV64IBS-NEXT:    ret
  %shr = lshr i32 %a, 5
  %and = and i32 %shr, 1
  ret i32 %and
}

define i64 @sbexti_i64(i64 %a) nounwind {
; RV64I-LABEL: sbexti_i64:
; RV64I:       # %bb.0:
; RV64I-NEXT:    srli a0, a0, 5
; RV64I-NEXT:    andi a0, a0, 1
; RV64I-NEXT:    ret
;
; RV64IB-LABEL: sbexti_i64:
; RV64IB:       # %bb.0:
; RV64IB-NEXT:    sbexti a0, a0, 5
; RV64IB-NEXT:    ret
;
; RV64IBS-LABEL: sbexti_i64:
; RV64IBS:       # %bb.0:
; RV64IBS-NEXT:    sbexti a0, a0, 5
; RV64IBS-NEXT:    ret
  %shr = lshr i64 %a, 5
  %and = and i64 %shr, 1
  ret i64 %and
}
