//===- AMDGPUSQTTFunctionInstrument.cpp -----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Insert the SQTT function entry and exit events in the functions marked with
// the "amdgpu-sqtt-instrument" or the "amdgpu-sqtt-instrument-inlined"
// attribute. As with the generic function entry/exit instrumentation, there are
// two variants of the pass:
//  - Pre-inlining: consumes "amdgpu-sqtt-instrument". It runs at the start of
//    the optimization pipeline, so the events of an inlined function end up in
//    its callers.
//  - Post-inlining: consumes "amdgpu-sqtt-instrument-inlined". It runs in the
//    codegen pipeline, after all the inlining, so only the functions that are
//    actually emitted are instrumented.
//
// The events describe the execution of a wave, but they are inserted on the
// thread CFG. If a function has divergent returns, the exit event of each of
// them would execute for the same wave with a different exec mask. To avoid
// it, the divergent returns of an instrumented function are first merged into a
// single return block, which holds their exit event. The returns that are
// reached through uniform branches are executed by the whole wave, so they
// keep their own exit event.
//
// The functions that are not kernels, have a single basic block, and have a
// code size lower than a threshold are not instrumented.
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "AMDGPUUnifyDivergentExitNodes.h"
#include "Utils/AMDGPUBaseInfo.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/UniformityAnalysis.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"
#include "llvm/IR/Metadata.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/AMDGPU/SQTTEvent.h"

using namespace llvm;

#define DEBUG_TYPE "amdgpu-sqtt-function-instrument"

namespace {

static cl::opt<unsigned> SkipThreshold(
    "amdgpu-sqtt-function-instrument-skip-threshold",
    cl::init(4 * TargetTransformInfo::TCC_Basic),
    cl::desc("Do not insert the entry and exit events in the functions with a "
             "single basic block and a code size lower than this threshold. 0 "
             "disables it."));

constexpr StringLiteral InstrumentAttr = "amdgpu-sqtt-instrument";
constexpr StringLiteral InstrumentInlinedAttr =
    "amdgpu-sqtt-instrument-inlined";

static StringRef getInstrumentAttr(bool PostInlining) {
  return PostInlining ? InstrumentInlinedAttr : InstrumentAttr;
}

// Whether \p F has to be instrumented by the variant of the pass selected by
// \p PostInlining. This is checked before getting any analysis.
static bool needsInstrumentation(const Function &F, bool PostInlining) {
  // Same as the generic entry/exit instrumentation: the asm of a naked function
  // can not be changed, and available_externally functions may not have a
  // definition outside of the module.
  if (F.isDeclaration() || F.hasFnAttribute(Attribute::Naked) ||
      F.hasAvailableExternallyLinkage())
    return false;

  return F.hasFnAttribute(getInstrumentAttr(PostInlining));
}

static void insertEvent(Function &F, sqtt::Event Event, Instruction *InsertPt,
                        const DebugLoc &DL) {
  LLVMContext &C = F.getContext();
  IRBuilder<> Builder(InsertPt);
  Builder.SetCurrentDebugLocation(DL);
  Builder.CreateIntrinsic(Intrinsic::amdgcn_sqtt_event,
                          {MetadataAsValue::get(C, Event.toMetadata(C)),
                           PoisonValue::get(Builder.getInt32Ty()),
                           /*IsCULocal=*/Builder.getTrue()});
}

static DebugLoc getEntryDebugLoc(const Function &F) {
  if (DISubprogram *SP = F.getSubprogram())
    return DILocation::get(SP->getContext(), SP->getScopeLine(), 0, SP);
  return DebugLoc();
}

static DebugLoc getExitDebugLoc(const Function &F, const Instruction &Ret) {
  if (DebugLoc DL = Ret.getDebugLoc())
    return DL;
  if (DISubprogram *SP = F.getSubprogram())
    return DILocation::get(SP->getContext(), 0, 0, SP);
  return DebugLoc();
}

// The instruction that the exit event is inserted before: the return or, if
// there is one, the musttail call that precedes it.
static Instruction *getExitInsertPt(BasicBlock &BB) {
  if (CallInst *CI = BB.getTerminatingMustTailCall())
    return CI;
  return BB.getTerminator();
}

static bool isTooSmallToInstrument(const Function &F,
                                   const TargetTransformInfo &TTI) {
  if (SkipThreshold == 0)
    return false;

  if (AMDGPU::isKernel(F))
    return false;

  if (F.size() != 1)
    return false;

  InstructionCost CodeSize = 0;
  for (const Instruction &I : F.getEntryBlock()) {
    CodeSize += TTI.getInstructionCost(&I, TargetTransformInfo::TCK_CodeSize);
    if (CodeSize >= SkipThreshold)
      return false;
  }
  return true;
}

static bool run(Function &F, bool PostInlining, const TargetTransformInfo &TTI,
                const UniformityInfo &UA, DominatorTree *DT) {
  assert(needsInstrumentation(F, PostInlining));

  // Consume the attribute so that the function is not instrumented twice.
  F.removeFnAttr(getInstrumentAttr(PostInlining));

  // A musttail call must stay next to its return, and a return that is
  // uniformly reached is executed by the whole wave. Such returns are not
  // merged.
  SmallVector<BasicBlock *> DivergentReturns;
  SmallVector<BasicBlock *> OtherReturns;
  for (BasicBlock &BB : F) {
    if (!isa<ReturnInst>(BB.getTerminator()))
      continue;
    if (BB.getTerminatingMustTailCall() || isUniformlyReached(UA, BB))
      OtherReturns.push_back(&BB);
    else
      DivergentReturns.push_back(&BB);
  }

  // Insert the entry event after the allocas: the SQTT lowering may later split
  // the entry block to guard the event, and the allocas left behind in the
  // split blocks would not be static.
  DebugLoc EntryDL = getEntryDebugLoc(F);
  insertEvent(F, sqtt::Event::functionEntry(F),
              &*F.getEntryBlock().getFirstNonPHIOrAlloca(), EntryDL);

  for (BasicBlock *BB : OtherReturns) {
    Instruction *InsertPt = getExitInsertPt(*BB);
    insertEvent(F, sqtt::Event::functionExit(F), InsertPt,
                getExitDebugLoc(F, *BB->getTerminator()));
  }

  if (DivergentReturns.empty())
    return true;

  DebugLoc ExitDL =
      getExitDebugLoc(F, *DivergentReturns.front()->getTerminator());
  BasicBlock *RetBlock = DivergentReturns.front();
  if (DivergentReturns.size() > 1) {
    DomTreeUpdater DTU(DT, DomTreeUpdater::UpdateStrategy::Eager);
    RetBlock = unifyReturnBlockSet(F, DT ? &DTU : nullptr, DivergentReturns,
                                   "UnifiedReturnBlock", TTI);
  }
  insertEvent(F, sqtt::Event::functionExit(F), RetBlock->getTerminator(),
              ExitDL);
  return true;
}

class AMDGPUSQTTFunctionInstrumentPostInlineLegacy : public FunctionPass {
public:
  static char ID;
  AMDGPUSQTTFunctionInstrumentPostInlineLegacy() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetTransformInfoWrapperPass>();
    AU.addRequired<UniformityInfoWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
  }

  bool runOnFunction(Function &F) override {
    constexpr bool PostInlining = true;
    if (!needsInstrumentation(F, PostInlining))
      return false;

    const auto &TTI = getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
    if (isTooSmallToInstrument(F, TTI)) {
      F.removeFnAttr(getInstrumentAttr(PostInlining));
      return true;
    }

    auto *DTWP = getAnalysisIfAvailable<DominatorTreeWrapperPass>();
    return ::run(F, /*PostInlining=*/true, TTI,
                 getAnalysis<UniformityInfoWrapperPass>().getUniformityInfo(),
                 DTWP ? &DTWP->getDomTree() : nullptr);
  }
};

} // namespace

char AMDGPUSQTTFunctionInstrumentPostInlineLegacy::ID = 0;

INITIALIZE_PASS_BEGIN(AMDGPUSQTTFunctionInstrumentPostInlineLegacy, DEBUG_TYPE,
                      "AMDGPU SQTT function instrument (post inlining)", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(UniformityInfoWrapperPass)
INITIALIZE_PASS_END(AMDGPUSQTTFunctionInstrumentPostInlineLegacy, DEBUG_TYPE,
                    "AMDGPU SQTT function instrument (post inlining)", false,
                    false)

FunctionPass *llvm::createAMDGPUSQTTFunctionInstrumentPostInlineLegacyPass() {
  return new AMDGPUSQTTFunctionInstrumentPostInlineLegacy();
}

PreservedAnalyses
AMDGPUSQTTFunctionInstrumentPass::run(Function &F,
                                      FunctionAnalysisManager &AM) {
  if (!needsInstrumentation(F, PostInlining))
    return PreservedAnalyses::all();

  const auto &TTI = AM.getResult<TargetIRAnalysis>(F);
  if (isTooSmallToInstrument(F, TTI)) {
    F.removeFnAttr(getInstrumentAttr(PostInlining));
    return PreservedAnalyses::all();
  }

  const auto &UA = AM.getResult<UniformityInfoAnalysis>(F);
  if (!::run(F, PostInlining, TTI, UA,
             AM.getCachedResult<DominatorTreeAnalysis>(F)))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserve<TargetIRAnalysis>();
  PA.preserve<DominatorTreeAnalysis>();
  return PA;
}

void AMDGPUSQTTFunctionInstrumentPass::printPipeline(
    raw_ostream &OS, function_ref<StringRef(StringRef)> MapClassName2PassName) {
  RequiredPassInfoMixin<AMDGPUSQTTFunctionInstrumentPass>::printPipeline(
      OS, MapClassName2PassName);
  OS << '<';
  if (PostInlining)
    OS << "post-inline";
  OS << '>';
}
