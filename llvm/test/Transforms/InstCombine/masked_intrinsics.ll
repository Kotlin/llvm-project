; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -instcombine -S < %s | FileCheck %s

declare <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptrs, i32, <2 x i1> %mask, <2 x double> %src0)
declare void @llvm.masked.store.v2f64.p0v2f64(<2 x double> %val, <2 x double>* %ptrs, i32, <2 x i1> %mask)
declare <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> %ptrs, i32, <2 x i1> %mask, <2 x double> %passthru)
declare <4 x double> @llvm.masked.gather.v4f64.v4p0f64(<4 x double*> %ptrs, i32, <4 x i1> %mask, <4 x double> %passthru)
declare void @llvm.masked.scatter.v2f64.v2p0f64(<2 x double> %val, <2 x double*> %ptrs, i32, <2 x i1> %mask)

define <2 x double> @load_zeromask(<2 x double>* %ptr, <2 x double> %passthru)  {
; CHECK-LABEL: @load_zeromask(
; CHECK-NEXT:    ret <2 x double> [[PASSTHRU:%.*]]
;
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 1, <2 x i1> zeroinitializer, <2 x double> %passthru)
  ret <2 x double> %res
}

define <2 x double> @load_onemask(<2 x double>* %ptr, <2 x double> %passthru)  {
; CHECK-LABEL: @load_onemask(
; CHECK-NEXT:    [[UNMASKEDLOAD:%.*]] = load <2 x double>, <2 x double>* [[PTR:%.*]], align 2
; CHECK-NEXT:    ret <2 x double> [[UNMASKEDLOAD]]
;
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 2, <2 x i1> <i1 1, i1 1>, <2 x double> %passthru)
  ret <2 x double> %res
}

define <2 x double> @load_undefmask(<2 x double>* %ptr, <2 x double> %passthru)  {
; CHECK-LABEL: @load_undefmask(
; CHECK-NEXT:    [[UNMASKEDLOAD:%.*]] = load <2 x double>, <2 x double>* [[PTR:%.*]], align 2
; CHECK-NEXT:    ret <2 x double> [[UNMASKEDLOAD]]
;
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 2, <2 x i1> <i1 1, i1 undef>, <2 x double> %passthru)
  ret <2 x double> %res
}

@G = external global i8

define <2 x double> @load_cemask(<2 x double>* %ptr, <2 x double> %passthru)  {
; CHECK-LABEL: @load_cemask(
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* [[PTR:%.*]], i32 2, <2 x i1> <i1 true, i1 ptrtoint (i8* @G to i1)>, <2 x double> [[PASSTHRU:%.*]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 2, <2 x i1> <i1 1, i1 ptrtoint (i8* @G to i1)>, <2 x double> %passthru)
  ret <2 x double> %res
}

define <2 x double> @load_lane0(<2 x double>* %ptr, double %pt)  {
; CHECK-LABEL: @load_lane0(
; CHECK-NEXT:    [[PTV2:%.*]] = insertelement <2 x double> undef, double [[PT:%.*]], i64 1
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* [[PTR:%.*]], i32 2, <2 x i1> <i1 true, i1 false>, <2 x double> [[PTV2]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  %ptv1 = insertelement <2 x double> undef, double %pt, i64 0
  %ptv2 = insertelement <2 x double> %ptv1, double %pt, i64 1
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 2, <2 x i1> <i1 true, i1 false>, <2 x double> %ptv2)
  ret <2 x double> %res
}

define double @load_all(double* %base, double %pt)  {
; CHECK-LABEL: @load_all(
; CHECK-NEXT:    [[PTRS:%.*]] = getelementptr double, double* [[BASE:%.*]], <4 x i64> <i64 0, i64 undef, i64 2, i64 3>
; CHECK-NEXT:    [[RES:%.*]] = call <4 x double> @llvm.masked.gather.v4f64.v4p0f64(<4 x double*> [[PTRS]], i32 4, <4 x i1> <i1 true, i1 false, i1 true, i1 true>, <4 x double> undef)
; CHECK-NEXT:    [[ELT:%.*]] = extractelement <4 x double> [[RES]], i64 2
; CHECK-NEXT:    ret double [[ELT]]
;
  %ptrs = getelementptr double, double* %base, <4 x i64> <i64 0, i64 1, i64 2, i64 3>
  %res = call <4 x double> @llvm.masked.gather.v4f64.v4p0f64(<4 x double*> %ptrs, i32 4, <4 x i1> <i1 true, i1 false, i1 true, i1 true>, <4 x double> undef)
  %elt = extractelement <4 x double> %res, i64 2
  ret double %elt
}

define <2 x double> @load_generic(<2 x double>* %ptr, double %pt,
; CHECK-LABEL: @load_generic(
; CHECK-NEXT:    [[PTV1:%.*]] = insertelement <2 x double> undef, double [[PT:%.*]], i64 0
; CHECK-NEXT:    [[PTV2:%.*]] = shufflevector <2 x double> [[PTV1]], <2 x double> undef, <2 x i32> zeroinitializer
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* [[PTR:%.*]], i32 4, <2 x i1> [[MASK:%.*]], <2 x double> [[PTV2]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  <2 x i1> %mask)  {
  %ptv1 = insertelement <2 x double> undef, double %pt, i64 0
  %ptv2 = insertelement <2 x double> %ptv1, double %pt, i64 1
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 4, <2 x i1> %mask, <2 x double> %ptv2)
  ret <2 x double> %res
}

define <2 x double> @load_speculative(<2 x double>* dereferenceable(16) align 4 %ptr,
; CHECK-LABEL: @load_speculative(
; CHECK-NEXT:    [[PTV1:%.*]] = insertelement <2 x double> undef, double [[PT:%.*]], i64 0
; CHECK-NEXT:    [[PTV2:%.*]] = shufflevector <2 x double> [[PTV1]], <2 x double> undef, <2 x i32> zeroinitializer
; CHECK-NEXT:    [[UNMASKEDLOAD:%.*]] = load <2 x double>, <2 x double>* [[PTR:%.*]], align 4
; CHECK-NEXT:    [[TMP1:%.*]] = select <2 x i1> [[MASK:%.*]], <2 x double> [[UNMASKEDLOAD]], <2 x double> [[PTV2]]
; CHECK-NEXT:    ret <2 x double> [[TMP1]]
;
  double %pt, <2 x i1> %mask)  {
  %ptv1 = insertelement <2 x double> undef, double %pt, i64 0
  %ptv2 = insertelement <2 x double> %ptv1, double %pt, i64 1
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 4, <2 x i1> %mask, <2 x double> %ptv2)
  ret <2 x double> %res
}

define <2 x double> @neg_load_spec_width(<2 x double>* dereferenceable(8) %ptr,
; CHECK-LABEL: @neg_load_spec_width(
; CHECK-NEXT:    [[PTV1:%.*]] = insertelement <2 x double> undef, double [[PT:%.*]], i64 0
; CHECK-NEXT:    [[PTV2:%.*]] = shufflevector <2 x double> [[PTV1]], <2 x double> undef, <2 x i32> zeroinitializer
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* nonnull [[PTR:%.*]], i32 4, <2 x i1> [[MASK:%.*]], <2 x double> [[PTV2]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  double %pt, <2 x i1> %mask)  {
  %ptv1 = insertelement <2 x double> undef, double %pt, i64 0
  %ptv2 = insertelement <2 x double> %ptv1, double %pt, i64 1
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 4, <2 x i1> %mask, <2 x double> %ptv2)
  ret <2 x double> %res
}

; Can't speculate since only half of required size is known deref
define <2 x double> @load_spec_neg_size(<2 x double>* dereferenceable(8) %ptr,
; CHECK-LABEL: @load_spec_neg_size(
; CHECK-NEXT:    [[PTV1:%.*]] = insertelement <2 x double> undef, double [[PT:%.*]], i64 0
; CHECK-NEXT:    [[PTV2:%.*]] = shufflevector <2 x double> [[PTV1]], <2 x double> undef, <2 x i32> zeroinitializer
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* nonnull [[PTR:%.*]], i32 4, <2 x i1> [[MASK:%.*]], <2 x double> [[PTV2]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  double %pt, <2 x i1> %mask)  {
  %ptv1 = insertelement <2 x double> undef, double %pt, i64 0
  %ptv2 = insertelement <2 x double> %ptv1, double %pt, i64 1
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 4, <2 x i1> %mask, <2 x double> %ptv2)
  ret <2 x double> %res
}

; Can only speculate one lane (but it's the only one active)
define <2 x double> @load_spec_lan0(<2 x double>* dereferenceable(8) %ptr,
; CHECK-LABEL: @load_spec_lan0(
; CHECK-NEXT:    [[PTV1:%.*]] = insertelement <2 x double> undef, double [[PT:%.*]], i64 0
; CHECK-NEXT:    [[PTV2:%.*]] = shufflevector <2 x double> [[PTV1]], <2 x double> undef, <2 x i32> zeroinitializer
; CHECK-NEXT:    [[MASK2:%.*]] = insertelement <2 x i1> [[MASK:%.*]], i1 false, i64 1
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* nonnull [[PTR:%.*]], i32 4, <2 x i1> [[MASK2]], <2 x double> [[PTV2]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  double %pt, <2 x i1> %mask)  {
  %ptv1 = insertelement <2 x double> undef, double %pt, i64 0
  %ptv2 = insertelement <2 x double> %ptv1, double %pt, i64 1
  %mask2 = insertelement <2 x i1> %mask, i1 false, i64 1
  %res = call <2 x double> @llvm.masked.load.v2f64.p0v2f64(<2 x double>* %ptr, i32 4, <2 x i1> %mask2, <2 x double> %ptv2)
  ret <2 x double> %res
}

define void @store_zeromask(<2 x double>* %ptr, <2 x double> %val)  {
; CHECK-LABEL: @store_zeromask(
; CHECK-NEXT:    ret void
;
  call void @llvm.masked.store.v2f64.p0v2f64(<2 x double> %val, <2 x double>* %ptr, i32 4, <2 x i1> zeroinitializer)
  ret void
}

define void @store_onemask(<2 x double>* %ptr, <2 x double> %val)  {
; CHECK-LABEL: @store_onemask(
; CHECK-NEXT:    store <2 x double> [[VAL:%.*]], <2 x double>* [[PTR:%.*]], align 4
; CHECK-NEXT:    ret void
;
  call void @llvm.masked.store.v2f64.p0v2f64(<2 x double> %val, <2 x double>* %ptr, i32 4, <2 x i1> <i1 1, i1 1>)
  ret void
}

define void @store_demandedelts(<2 x double>* %ptr, double %val)  {
; CHECK-LABEL: @store_demandedelts(
; CHECK-NEXT:    [[VALVEC2:%.*]] = insertelement <2 x double> undef, double [[VAL:%.*]], i32 0
; CHECK-NEXT:    call void @llvm.masked.store.v2f64.p0v2f64(<2 x double> [[VALVEC2]], <2 x double>* [[PTR:%.*]], i32 4, <2 x i1> <i1 true, i1 false>)
; CHECK-NEXT:    ret void
;
  %valvec1 = insertelement <2 x double> undef, double %val, i32 0
  %valvec2 = insertelement <2 x double> %valvec1, double %val, i32 1
  call void @llvm.masked.store.v2f64.p0v2f64(<2 x double> %valvec2, <2 x double>* %ptr, i32 4, <2 x i1> <i1 true, i1 false>)
  ret void
}

define <2 x double> @gather_generic(<2 x double*> %ptrs, <2 x i1> %mask,
; CHECK-LABEL: @gather_generic(
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> [[PTRS:%.*]], i32 4, <2 x i1> [[MASK:%.*]], <2 x double> [[PASSTHRU:%.*]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  <2 x double> %passthru)  {
  %res = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> %ptrs, i32 4, <2 x i1> %mask, <2 x double> %passthru)
  ret <2 x double> %res
}


define <2 x double> @gather_zeromask(<2 x double*> %ptrs, <2 x double> %passthru)  {
; CHECK-LABEL: @gather_zeromask(
; CHECK-NEXT:    ret <2 x double> [[PASSTHRU:%.*]]
;
  %res = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> %ptrs, i32 4, <2 x i1> zeroinitializer, <2 x double> %passthru)
  ret <2 x double> %res
}


define <2 x double> @gather_onemask(<2 x double*> %ptrs, <2 x double> %passthru)  {
; CHECK-LABEL: @gather_onemask(
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> [[PTRS:%.*]], i32 4, <2 x i1> <i1 true, i1 true>, <2 x double> undef)
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  %res = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> %ptrs, i32 4, <2 x i1> <i1 true, i1 true>, <2 x double> %passthru)
  ret <2 x double> %res
}

define <4 x double> @gather_lane2(double* %base, double %pt)  {
; CHECK-LABEL: @gather_lane2(
; CHECK-NEXT:    [[PTRS:%.*]] = getelementptr double, double* [[BASE:%.*]], <4 x i64> <i64 undef, i64 undef, i64 2, i64 undef>
; CHECK-NEXT:    [[PT_V1:%.*]] = insertelement <4 x double> undef, double [[PT:%.*]], i64 0
; CHECK-NEXT:    [[PT_V2:%.*]] = shufflevector <4 x double> [[PT_V1]], <4 x double> undef, <4 x i32> <i32 0, i32 0, i32 undef, i32 0>
; CHECK-NEXT:    [[RES:%.*]] = call <4 x double> @llvm.masked.gather.v4f64.v4p0f64(<4 x double*> [[PTRS]], i32 4, <4 x i1> <i1 false, i1 false, i1 true, i1 false>, <4 x double> [[PT_V2]])
; CHECK-NEXT:    ret <4 x double> [[RES]]
;
  %ptrs = getelementptr double, double *%base, <4 x i64> <i64 0, i64 1, i64 2, i64 3>
  %pt_v1 = insertelement <4 x double> undef, double %pt, i64 0
  %pt_v2 = shufflevector <4 x double> %pt_v1, <4 x double> undef, <4 x i32> zeroinitializer
  %res = call <4 x double> @llvm.masked.gather.v4f64.v4p0f64(<4 x double*> %ptrs, i32 4, <4 x i1> <i1 false, i1 false, i1 true, i1 false>, <4 x double> %pt_v2)
  ret <4 x double> %res
}

define <2 x double> @gather_lane0_maybe(double* %base, double %pt,
; CHECK-LABEL: @gather_lane0_maybe(
; CHECK-NEXT:    [[PTRS:%.*]] = getelementptr double, double* [[BASE:%.*]], <2 x i64> <i64 0, i64 1>
; CHECK-NEXT:    [[PT_V1:%.*]] = insertelement <2 x double> undef, double [[PT:%.*]], i64 0
; CHECK-NEXT:    [[PT_V2:%.*]] = shufflevector <2 x double> [[PT_V1]], <2 x double> undef, <2 x i32> zeroinitializer
; CHECK-NEXT:    [[MASK2:%.*]] = insertelement <2 x i1> [[MASK:%.*]], i1 false, i64 1
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> [[PTRS]], i32 4, <2 x i1> [[MASK2]], <2 x double> [[PT_V2]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  <2 x i1> %mask)  {
  %ptrs = getelementptr double, double *%base, <2 x i64> <i64 0, i64 1>
  %pt_v1 = insertelement <2 x double> undef, double %pt, i64 0
  %pt_v2 = insertelement <2 x double> %pt_v1, double %pt, i64 1
  %mask2 = insertelement <2 x i1> %mask, i1 false, i64 1
  %res = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> %ptrs, i32 4, <2 x i1> %mask2, <2 x double> %pt_v2)
  ret <2 x double> %res
}

define <2 x double> @gather_lane0_maybe_spec(double* %base, double %pt,
; CHECK-LABEL: @gather_lane0_maybe_spec(
; CHECK-NEXT:    [[PTRS:%.*]] = getelementptr double, double* [[BASE:%.*]], <2 x i64> <i64 0, i64 1>
; CHECK-NEXT:    [[PT_V1:%.*]] = insertelement <2 x double> undef, double [[PT:%.*]], i64 0
; CHECK-NEXT:    [[PT_V2:%.*]] = shufflevector <2 x double> [[PT_V1]], <2 x double> undef, <2 x i32> zeroinitializer
; CHECK-NEXT:    [[MASK2:%.*]] = insertelement <2 x i1> [[MASK:%.*]], i1 false, i64 1
; CHECK-NEXT:    [[RES:%.*]] = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> [[PTRS]], i32 4, <2 x i1> [[MASK2]], <2 x double> [[PT_V2]])
; CHECK-NEXT:    ret <2 x double> [[RES]]
;
  <2 x i1> %mask)  {
  %ptrs = getelementptr double, double *%base, <2 x i64> <i64 0, i64 1>
  %pt_v1 = insertelement <2 x double> undef, double %pt, i64 0
  %pt_v2 = insertelement <2 x double> %pt_v1, double %pt, i64 1
  %mask2 = insertelement <2 x i1> %mask, i1 false, i64 1
  %res = call <2 x double> @llvm.masked.gather.v2f64.v2p0f64(<2 x double*> %ptrs, i32 4, <2 x i1> %mask2, <2 x double> %pt_v2)
  ret <2 x double> %res
}


define void @scatter_zeromask(<2 x double*> %ptrs, <2 x double> %val)  {
; CHECK-LABEL: @scatter_zeromask(
; CHECK-NEXT:    ret void
;
  call void @llvm.masked.scatter.v2f64.v2p0f64(<2 x double> %val, <2 x double*> %ptrs, i32 8, <2 x i1> zeroinitializer)
  ret void
}

define void @scatter_demandedelts(double* %ptr, double %val)  {
; CHECK-LABEL: @scatter_demandedelts(
; CHECK-NEXT:    [[PTRS:%.*]] = getelementptr double, double* [[PTR:%.*]], <2 x i64> <i64 0, i64 undef>
; CHECK-NEXT:    [[VALVEC2:%.*]] = insertelement <2 x double> undef, double [[VAL:%.*]], i32 0
; CHECK-NEXT:    call void @llvm.masked.scatter.v2f64.v2p0f64(<2 x double> [[VALVEC2]], <2 x double*> [[PTRS]], i32 8, <2 x i1> <i1 true, i1 false>)
; CHECK-NEXT:    ret void
;
  %ptrs = getelementptr double, double* %ptr, <2 x i64> <i64 0, i64 1>
  %valvec1 = insertelement <2 x double> undef, double %val, i32 0
  %valvec2 = insertelement <2 x double> %valvec1, double %val, i32 1
  call void @llvm.masked.scatter.v2f64.v2p0f64(<2 x double> %valvec2, <2 x double*> %ptrs, i32 8, <2 x i1> <i1 true, i1 false>)
  ret void
}
