; RUN: opt %loadPolly -polly-codegen-ppcg -polly-acc-dump-kernel-ir \
; RUN: -disable-output < %s | \
; RUN: FileCheck -check-prefix=KERNEL %s
;
; REQUIRES: pollyacc
;
;    void kernel_params_only_some_arrays(float A[], float B[]) {
;      for (long i = 0; i < 32; i++)
;        A[i] += 42;
;
;      for (long i = 0; i < 32; i++)
;        B[i] += 42;
;    }

; KERNEL: ; ModuleID = 'kernel_0'
; KERNEL-NEXT: source_filename = "kernel_0"
; KERNEL-NEXT: target datalayout = "e-i64:64-v16:16-v32:32-n16:32:64"
; KERNEL-NEXT: target triple = "nvptx64-nvidia-cuda"

; KERNEL: define ptx_kernel void @kernel_0(i8* %MemRef_A) {
; KERNEL-NEXT:   entry:
; KERNEL-NEXT:     %0 = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()
; KERNEL-NEXT:     %b0 = zext i32 %0 to i64
; KERNEL-NEXT:     %1 = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()
; KERNEL-NEXT:     %t0 = zext i32 %1 to i64

; KERNEL:     ret void
; KERNEL-NEXT: }

; KERNEL: ; ModuleID = 'kernel_1'
; KERNEL-NEXT: source_filename = "kernel_1"
; KERNEL-NEXT: target datalayout = "e-i64:64-v16:16-v32:32-n16:32:64"
; KERNEL-NEXT: target triple = "nvptx64-nvidia-cuda"

; KERNEL: define ptx_kernel void @kernel_1(i8* %MemRef_B) {
; KERNEL-NEXT:   entry:
; KERNEL-NEXT:     %0 = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()
; KERNEL-NEXT:     %b0 = zext i32 %0 to i64
; KERNEL-NEXT:     %1 = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()
; KERNEL-NEXT:     %t0 = zext i32 %1 to i64

; KERNEL:     ret void
; KERNEL-NEXT: }

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"

define void @kernel_params_only_some_arrays(float* %A, float* %B) {
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %i.0 = phi i64 [ 0, %entry ], [ %inc, %for.inc ]
  %exitcond1 = icmp ne i64 %i.0, 32
  br i1 %exitcond1, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %arrayidx = getelementptr inbounds float, float* %A, i64 %i.0
  %tmp = load float, float* %arrayidx, align 4
  %add = fadd float %tmp, 4.200000e+01
  store float %add, float* %arrayidx, align 4
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %inc = add nuw nsw i64 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  br label %for.cond2

for.cond2:                                        ; preds = %for.inc7, %for.end
  %i1.0 = phi i64 [ 0, %for.end ], [ %inc8, %for.inc7 ]
  %exitcond = icmp ne i64 %i1.0, 32
  br i1 %exitcond, label %for.body4, label %for.end9

for.body4:                                        ; preds = %for.cond2
  %arrayidx5 = getelementptr inbounds float, float* %B, i64 %i1.0
  %tmp2 = load float, float* %arrayidx5, align 4
  %add6 = fadd float %tmp2, 4.200000e+01
  store float %add6, float* %arrayidx5, align 4
  br label %for.inc7

for.inc7:                                         ; preds = %for.body4
  %inc8 = add nuw nsw i64 %i1.0, 1
  br label %for.cond2

for.end9:                                         ; preds = %for.cond2
  ret void
}
