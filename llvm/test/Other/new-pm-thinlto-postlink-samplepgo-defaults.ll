; Validate ThinLTO postlink pipeline when we have Sample PGO
;
; RUN: opt -disable-verify -debug-pass-manager \
; RUN:     -pgo-kind=pgo-sample-use-pipeline -profile-file='%S/Inputs/new-pm-thinlto-samplepgo-defaults.prof' \
; RUN:     -passes='thinlto<O1>' -S %s 2>&1 \
; RUN:     | FileCheck %s --check-prefixes=CHECK-O,CHECK-O1,%llvmcheckext --dump-input=fail
; RUN: opt -disable-verify -debug-pass-manager \
; RUN:     -pgo-kind=pgo-sample-use-pipeline -profile-file='%S/Inputs/new-pm-thinlto-samplepgo-defaults.prof' \
; RUN:     -passes='thinlto<O2>' -S  %s 2>&1 \
; RUN:     | FileCheck %s --check-prefixes=CHECK-O,CHECK-O2,CHECK-O23SZ,%llvmcheckext --dump-input=fail
; RUN: opt -disable-verify -debug-pass-manager -passes-ep-pipeline-start='no-op-module' \
; RUN:     -pgo-kind=pgo-sample-use-pipeline -profile-file='%S/Inputs/new-pm-thinlto-samplepgo-defaults.prof' \
; RUN:     -passes='thinlto<O3>' -S  %s 2>&1 \
; RUN:     | FileCheck %s --check-prefixes=CHECK-O,CHECK-O3,CHECK-O23SZ,%llvmcheckext --dump-input=fail
; RUN: opt -disable-verify -debug-pass-manager \
; RUN:     -pgo-kind=pgo-sample-use-pipeline -profile-file='%S/Inputs/new-pm-thinlto-samplepgo-defaults.prof' \
; RUN:     -passes='thinlto<Os>' -S %s 2>&1 \
; RUN:     | FileCheck %s --check-prefixes=CHECK-O,CHECK-Os,CHECK-O23SZ,%llvmcheckext --dump-input=fail
; RUN: opt -disable-verify -debug-pass-manager \
; RUN:     -pgo-kind=pgo-sample-use-pipeline -profile-file='%S/Inputs/new-pm-thinlto-samplepgo-defaults.prof' \
; RUN:     -passes='thinlto<Oz>' -S %s 2>&1 \
; RUN:     | FileCheck %s --check-prefixes=CHECK-O,CHECK-Oz,CHECK-O23SZ,%llvmcheckext --dump-input=fail
; RUN: opt -disable-verify -debug-pass-manager -new-pm-debug-info-for-profiling \
; RUN:     -pgo-kind=pgo-sample-use-pipeline -profile-file='%S/Inputs/new-pm-thinlto-samplepgo-defaults.prof' \
; RUN:     -passes='thinlto<O2>' -S  %s 2>&1 \
; RUN:     | FileCheck %s --check-prefixes=CHECK-O,CHECK-O2,CHECK-O23SZ,%llvmcheckext --dump-input=fail
;
; CHECK-O: Running analysis: PassInstrumentationAnalysis
; CHECK-O-NEXT: Starting {{.*}}Module pass manager run.
; CHECK-O-NEXT: Running pass: PassManager<{{.*}}Module{{.*}}>
; CHECK-O-NEXT: Starting {{.*}}Module pass manager run.
; CHECK-O-NEXT: Running pass: ForceFunctionAttrsPass
; CHECK-EP-PIPELINE-START-NEXT: Running pass: NoOpModulePass
; CHECK-O-NEXT: Running pass: PassManager<{{.*}}Module{{.*}}>
; CHECK-O-NEXT: Starting {{.*}}Module pass manager run.
; CHECK-O-NEXT: Running pass: InferFunctionAttrsPass
; CHECK-O-NEXT: Running analysis: InnerAnalysisManagerProxy
; CHECK-O-NEXT: Running analysis: TargetLibraryAnalysis
; CHECK-O-NEXT: Running analysis: PassInstrumentationAnalysis
; CHECK-O-NEXT: Running pass: ModuleToFunctionPassAdaptor<{{.*}}PassManager{{.*}}>
; CHECK-O-NEXT: Running analysis: PassInstrumentationAnalysis
; CHECK-O-NEXT: Starting {{.*}}Function pass manager run.
; CHECK-O-NEXT: Running pass: SimplifyCFGPass
; CHECK-O-NEXT: Running analysis: TargetIRAnalysis
; CHECK-O-NEXT: Running analysis: AssumptionAnalysis
; CHECK-O-NEXT: Running pass: SROA
; CHECK-O-NEXT: Running analysis: DominatorTreeAnalysis
; CHECK-O-NEXT: Running pass: EarlyCSEPass
; CHECK-O-NEXT: Running analysis: TargetLibraryAnalysis
; CHECK-O-NEXT: Running pass: LowerExpectIntrinsicPass
; CHECK-O3-NEXT: Running pass: CallSiteSplittingPass
; CHECK-O-NEXT: Running pass: InstCombinePass on foo
; CHECK-O-NEXT: Running analysis: OptimizationRemarkEmitterAnalysis on foo
; CHECK-O-NEXT: Running analysis: AAManager on foo
; CHECK-O-NEXT: Running analysis: OuterAnalysisManagerProxy
; CHECK-O-NEXT: Finished {{.*}}Function pass manager run.
; CHECK-O-NEXT: Running pass: SampleProfileLoaderPass
; CHECK-O-NEXT: Running analysis: ProfileSummaryAnalysis
; CHECK-O-NEXT: Running analysis: CallGraphAnalysis
; CHECK-O-NEXT: Running pass: RequireAnalysisPass<{{.*}}ProfileSummaryAnalysis
; CHECK-O-NEXT: Running pass: PGOIndirectCallPromotion
; CHECK-O-NEXT: Running pass: LowerTypeTestsPass
; CHECK-O-NEXT: Running pass: IPSCCPPass
; CHECK-O-NEXT: Running pass: CalledValuePropagationPass
; CHECK-O-NEXT: Running pass: GlobalOptPass
; CHECK-O-NEXT: Running pass: ModuleToFunctionPassAdaptor<{{.*}}PromotePass>
; CHECK-O-NEXT: Running pass: DeadArgumentEliminationPass
; CHECK-O-NEXT: Running pass: ModuleToFunctionPassAdaptor<{{.*}}PassManager{{.*}}>
; CHECK-O-NEXT: Starting {{.*}}Function pass manager run.
; CHECK-O-NEXT: Running pass: InstCombinePass
; CHECK-O-NEXT: Running analysis: BlockFrequencyAnalysis on foo
; These next two can appear in any order since they are accessed as parameters
; on the same call to BlockFrequencyInfo::calculate.
; CHECK-O-DAG: Running analysis: LoopAnalysis on foo
; CHECK-O-DAG: Running analysis: BranchProbabilityAnalysis on foo
; CHECK-O-NEXT: Running analysis: PostDominatorTreeAnalysis on foo
; CHECK-O-NEXT: Running pass: SimplifyCFGPass on foo
; CHECK-O-NEXT: Finished {{.*}}Function pass manager run
; CHECK-O-NEXT: Running pass: ModuleInlinerWrapperPass
; CHECK-O-NEXT: Running analysis: InlineAdvisorAnalysis
; CHECK-O-NEXT: Starting {{.*}}Module pass manager run.
; CHECK-O-NEXT: Running pass: RequireAnalysisPass<{{.*}}GlobalsAA
; CHECK-O-NEXT: Running analysis: GlobalsAA
; CHECK-O-NEXT: Running pass: RequireAnalysisPass<{{.*}}ProfileSummaryAnalysis
; CHECK-O-NEXT: Running pass: ModuleToPostOrderCGSCCPassAdaptor<{{.*}}LazyCallGraph{{.*}}>
; CHECK-O-NEXT: Running analysis: InnerAnalysisManagerProxy
; CHECK-O-NEXT: Running analysis: LazyCallGraphAnalysis
; CHECK-O-NEXT: Running analysis: FunctionAnalysisManagerCGSCCProxy
; CHECK-O-NEXT: Running analysis: PassInstrumentationAnalysis
; CHECK-O-NEXT: Running analysis: OuterAnalysisManagerProxy<{{.*}}LazyCallGraph::SCC{{.*}}>
; CHECK-O-NEXT: Starting CGSCC pass manager run.
; CHECK-O-NEXT: Running pass: InlinerPass
; CHECK-O-NEXT: Running pass: PostOrderFunctionAttrsPass
; CHECK-O3-NEXT: Running pass: ArgumentPromotionPass
; CHECK-O2-NEXT: Running pass: OpenMPOptPass
; CHECK-O3-NEXT: Running pass: OpenMPOptPass
; CHECK-O-NEXT: Running pass: CGSCCToFunctionPassAdaptor<{{.*}}PassManager{{.*}}>
; CHECK-O-NEXT: Starting {{.*}}Function pass manager run.
; CHECK-O-NEXT: Running pass: SROA
; CHECK-O-NEXT: Running pass: EarlyCSEPass
; CHECK-O-NEXT: Running analysis: MemorySSAAnalysis
; CHECK-O23SZ-NEXT: Running pass: SpeculativeExecutionPass
; CHECK-O23SZ-NEXT: Running pass: JumpThreadingPass
; CHECK-O23SZ-NEXT: Running analysis: LazyValueAnalysis
; CHECK-O23SZ-NEXT: Running pass: CorrelatedValuePropagationPass
; CHECK-O-NEXT: Running pass: SimplifyCFGPass
; CHECK-O3-NEXT: Running pass: AggressiveInstCombinePass
; CHECK-O-NEXT: Running pass: InstCombinePass
; CHECK-O1-NEXT: Running pass: LibCallsShrinkWrapPass
; CHECK-O2-NEXT: Running pass: LibCallsShrinkWrapPass
; CHECK-O3-NEXT: Running pass: LibCallsShrinkWrapPass
; CHECK-O23SZ-NEXT: Running pass: TailCallElimPass
; CHECK-O-NEXT: Running pass: SimplifyCFGPass
; CHECK-O-NEXT: Running pass: ReassociatePass
; CHECK-O-NEXT: Running pass: RequireAnalysisPass<{{.*}}OptimizationRemarkEmitterAnalysis
; CHECK-O-NEXT: Running pass: FunctionToLoopPassAdaptor<{{.*}}LoopStandardAnalysisResults{{.*}}>
; CHECK-O-NEXT: Starting {{.*}}Function pass manager run
; CHECK-O-NEXT: Running pass: LoopSimplifyPass
; CHECK-O-NEXT: Running pass: LCSSAPass
; CHECK-O-NEXT: Finished {{.*}}Function pass manager run
; CHECK-O-NEXT: Running analysis: ScalarEvolutionAnalysis
; CHECK-O-NEXT: Running analysis: InnerAnalysisManagerProxy
; CHECK-O-NEXT: Starting Loop pass manager run.
; CHECK-O-NEXT: Running analysis: PassInstrumentationAnalysis
; CHECK-O-NEXT: Running pass: LoopInstSimplifyPass
; CHECK-O-NEXT: Running pass: LoopSimplifyCFGPass
; CHECK-O-NEXT: Running pass: LoopRotatePass
; CHECK-O-NEXT: Running pass: LICM
; CHECK-O-NEXT: Running pass: SimpleLoopUnswitchPass
; CHECK-O-NEXT: Finished Loop pass manager run.
; CHECK-O-NEXT: Running pass: SimplifyCFGPass
; CHECK-O-NEXT: Running pass: InstCombinePass
; CHECK-O-NEXT: Running pass: FunctionToLoopPassAdaptor<{{.*}}LoopStandardAnalysisResults{{.*}}>
; CHECK-O-NEXT: Starting {{.*}}Function pass manager run
; CHECK-O-NEXT: Running pass: LoopSimplifyPass
; CHECK-O-NEXT: Running pass: LCSSAPass
; CHECK-O-NEXT: Finished {{.*}}Function pass manager run
; CHECK-O-NEXT: Starting Loop pass manager run.
; CHECK-O-NEXT: Running pass: IndVarSimplifyPass
; CHECK-O-NEXT: Running pass: LoopIdiomRecognizePass
; CHECK-O-NEXT: Running pass: LoopDeletionPass
; CHECK-O-NEXT: Running pass: LoopFullUnrollPass
; CHECK-O-NEXT: Finished Loop pass manager run.
; CHECK-O-NEXT: Running pass: SROA on foo
; CHECK-Os-NEXT: Running pass: MergedLoadStoreMotionPass
; CHECK-Os-NEXT: Running pass: GVN
; CHECK-Os-NEXT: Running analysis: MemoryDependenceAnalysis
; CHECK-Os-NEXT: Running analysis: PhiValuesAnalysis
; CHECK-Oz-NEXT: Running pass: MergedLoadStoreMotionPass
; CHECK-Oz-NEXT: Running pass: GVN
; CHECK-Oz-NEXT: Running analysis: MemoryDependenceAnalysis
; CHECK-Oz-NEXT: Running analysis: PhiValuesAnalysis
; CHECK-O2-NEXT: Running pass: MergedLoadStoreMotionPass
; CHECK-O2-NEXT: Running pass: GVN
; CHECK-O2-NEXT: Running analysis: MemoryDependenceAnalysis
; CHECK-O2-NEXT: Running analysis: PhiValuesAnalysis
; CHECK-O3-NEXT: Running pass: MergedLoadStoreMotionPass
; CHECK-O3-NEXT: Running pass: GVN
; CHECK-O3-NEXT: Running analysis: MemoryDependenceAnalysis
; CHECK-O3-NEXT: Running analysis: PhiValuesAnalysis
; CHECK-O-NEXT: Running pass: MemCpyOptPass
; CHECK-O1-NEXT: Running analysis: MemoryDependenceAnalysis
; CHECK-O1-NEXT: Running analysis: PhiValuesAnalysis
; CHECK-O-NEXT: Running pass: SCCPPass
; CHECK-O-NEXT: Running pass: BDCEPass
; CHECK-O-NEXT: Running analysis: DemandedBitsAnalysis
; CHECK-O-NEXT: Running pass: InstCombinePass
; CHECK-O23SZ-NEXT: Running pass: JumpThreadingPass
; CHECK-O23SZ-NEXT: Running pass: CorrelatedValuePropagationPass
; CHECK-O23SZ-NEXT: Running pass: DSEPass
; CHECK-O23SZ-NEXT: Running pass: FunctionToLoopPassAdaptor<{{.*}}LICMPass{{.*}}>
; CHECK-O23SZ-NEXT: Starting {{.*}}Function pass manager run
; CHECK-O23SZ-NEXT: Running pass: LoopSimplifyPass
; CHECK-O23SZ-NEXT: Running pass: LCSSAPass
; CHECK-O23SZ-NEXT: Finished {{.*}}Function pass manager run
; CHECK-O-NEXT: Running pass: ADCEPass
; CHECK-O-NEXT: Running pass: SimplifyCFGPass
; CHECK-O-NEXT: Running pass: InstCombinePass
; CHECK-O3-NEXT: Running pass: ControlHeightReductionPass on foo
; CHECK-O3-NEXT: Running analysis: RegionInfoAnalysis on foo
; CHECK-O3-NEXT: Running analysis: DominanceFrontierAnalysis on foo
; CHECK-O-NEXT: Finished {{.*}}Function pass manager run.
; CHECK-O-NEXT: Finished CGSCC pass manager run.
; CHECK-O-NEXT: Finished {{.*}}Module pass manager run.
; CHECK-O-NEXT: Finished {{.*}}Module pass manager run.
; CHECK-O-NEXT: Running pass: PassManager<{{.*}}Module{{.*}}>
; CHECK-O-NEXT: Starting {{.*}}Module pass manager run.
; CHECK-O-NEXT: Running pass: GlobalOptPass
; CHECK-O-NEXT: Running pass: GlobalDCEPass
; CHECK-O-NEXT: Running pass: EliminateAvailableExternallyPass
; CHECK-O-NEXT: Running pass: ReversePostOrderFunctionAttrsPass
; CHECK-O-NEXT: Running pass: RequireAnalysisPass<{{.*}}GlobalsAA
; CHECK-O-NEXT: Running pass: ModuleToFunctionPassAdaptor<{{.*}}PassManager{{.*}}>
; CHECK-O-NEXT: Starting {{.*}}Function pass manager run.
; CHECK-O-NEXT: Running pass: Float2IntPass
; CHECK-O-NEXT: Running pass: LowerConstantIntrinsicsPass
; CHECK-EXT: Running pass: {{.*}}::Bye
; CHECK-O-NEXT: Running pass: FunctionToLoopPassAdaptor<{{.*}}LoopRotatePass
; CHECK-O-NEXT: Starting {{.*}}Function pass manager run
; CHECK-O-NEXT: Running pass: LoopSimplifyPass
; CHECK-O-NEXT: Running pass: LCSSAPass
; CHECK-O-NEXT: Finished {{.*}}Function pass manager run
; CHECK-O-NEXT: Running pass: LoopDistributePass
; CHECK-O-NEXT: Running pass: InjectTLIMappings
; CHECK-O-NEXT: Running pass: LoopVectorizePass
; CHECK-O-NEXT: Running pass: LoopLoadEliminationPass
; CHECK-O-NEXT: Running analysis: LoopAccessAnalysis
; CHECK-O-NEXT: Running pass: InstCombinePass
; CHECK-O-NEXT: Running pass: SimplifyCFGPass
; CHECK-O2-NEXT: Running pass: SLPVectorizerPass
; CHECK-O3-NEXT: Running pass: SLPVectorizerPass
; CHECK-Os-NEXT: Running pass: SLPVectorizerPass
; CHECK-O-NEXT: Running pass: VectorCombinePass
; CHECK-O-NEXT: Running pass: InstCombinePass
; CHECK-O-NEXT: Running pass: LoopUnrollPass
; CHECK-O-NEXT: Running pass: WarnMissedTransformationsPass
; CHECK-O-NEXT: Running pass: InstCombinePass
; CHECK-O-NEXT: Running pass: RequireAnalysisPass<{{.*}}OptimizationRemarkEmitterAnalysis
; CHECK-O-NEXT: Running pass: FunctionToLoopPassAdaptor<{{.*}}LICMPass
; CHECK-O-NEXT: Starting {{.*}}Function pass manager run
; CHECK-O-NEXT: Running pass: LoopSimplifyPass
; CHECK-O-NEXT: Running pass: LCSSAPass
; CHECK-O-NEXT: Finished {{.*}}Function pass manager run
; CHECK-O-NEXT: Running pass: AlignmentFromAssumptionsPass
; CHECK-O-NEXT: Running pass: LoopSinkPass
; CHECK-O-NEXT: Running pass: InstSimplifyPass
; CHECK-O-NEXT: Running pass: DivRemPairsPass
; CHECK-O-NEXT: Running pass: SimplifyCFGPass
; CHECK-O-NEXT: Running pass: SpeculateAroundPHIsPass
; CHECK-O-NEXT: Finished {{.*}}Function pass manager run.
; CHECK-O-NEXT: Running pass: CGProfilePass
; CHECK-O-NEXT: Running pass: GlobalDCEPass
; CHECK-O-NEXT: Running pass: ConstantMergePass
; CHECK-O-NEXT: Finished {{.*}}Module pass manager run.
; CHECK-O-NEXT: Running pass: ModuleToFunctionPassAdaptor
; CHECK-O-NEXT: Finished {{.*}}Module pass manager run.
; CHECK-O-NEXT: Running pass: PrintModulePass

; Make sure we get the IR back out without changes when we print the module.
; CHECK-O-LABEL: define void @foo(i32 %n) local_unnamed_addr
; CHECK-O-NEXT: entry:
; CHECK-O-NEXT:   br label %loop
; CHECK-O:      loop:
; CHECK-O-NEXT:   %iv = phi i32 [ 0, %entry ], [ %iv.next, %loop ]
; CHECK-O-NEXT:   %iv.next = add i32 %iv, 1
; CHECK-O-NEXT:   tail call void @bar()
; CHECK-O-NEXT:   %cmp = icmp eq i32 %iv, %n
; CHECK-O-NEXT:   br i1 %cmp, label %exit, label %loop
; CHECK-O:      exit:
; CHECK-O-NEXT:   ret void
; CHECK-O-NEXT: }
;
; Ignore a bunch of intervening metadata containing profile data.
;
; CHECK-O: Finished {{.*}}Module pass manager run.

declare void @bar() local_unnamed_addr

define void @foo(i32 %n) local_unnamed_addr {
entry:
  br label %loop
loop:
  %iv = phi i32 [ 0, %entry ], [ %iv.next, %loop ]
  %iv.next = add i32 %iv, 1
  tail call void @bar()
  %cmp = icmp eq i32 %iv, %n
  br i1 %cmp, label %exit, label %loop
exit:
  ret void
}
