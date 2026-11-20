//===- AMDGPUSQTTLower.cpp ------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Optimize llvm.amdgcn.sqtt.event uses and allocate unique event IDs.
//
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "AMDGPUTargetMachine.h"
#include "GCNSubtarget.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"
#include "llvm/IR/PassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/AMDGPU/SQTTEvent.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include <algorithm>

#define DEBUG_TYPE "amdgpu-sqtt-lower"

using namespace llvm;

namespace {
static cl::opt<unsigned>
    CombineEventsThreshold("amdgpu-sqtt-combine-events-threshold",
                           cl::init(4 * TargetTransformInfo::TCC_Basic),
                           cl::desc("Combine consecutive SQTT events separated "
                                    "by a latency of at most this threshold."));

// Bit N is set if the waves running on SIMD N execute the SQTT events.
static cl::opt<uint32_t>
    SIMDMask("amdgpu-sqtt-simd-mask",
             cl::init(std::numeric_limits<uint32_t>::max()),
             cl::desc("Bitmask of the SIMDs whose waves execute the SQTT "
                      "events (bit N enables SIMD N). Bits above the number "
                      "of SIMDs are ignored."));

// Bit N is set if the waves that occupy the slot N of their SIMD execute the
// SQTT events.
static cl::opt<uint32_t>
    WaveMask("amdgpu-sqtt-wave-mask",
             cl::init(std::numeric_limits<uint32_t>::max()),
             cl::desc("Bitmask of the wave slots whose waves execute the SQTT "
                      "events (bit N enables slot N). Bits above the number "
                      "of slots are ignored."));

// Bit N is set if the waves running on the compute unit N (work-group processor
// N on GFX10+) execute the SQTT events.
static cl::opt<uint32_t>
    CUMask("amdgpu-sqtt-cu-mask",
           cl::init(std::numeric_limits<uint32_t>::max()),
           cl::desc("Bitmask of the compute units whose waves execute the SQTT "
                    "events (bit N enables CU N, or WGP N on GFX10+). Bits "
                    "above the number of CUs are ignored."));

// Bit N is set if the waves of the work-groups that occupy the slot N of their
// compute unit execute the SQTT events.
static cl::opt<uint32_t>
    WGMask("amdgpu-sqtt-wg-mask",
           cl::init(std::numeric_limits<uint32_t>::max()),
           cl::desc("Bitmask of the work-group slots whose waves execute the "
                    "SQTT events (bit N enables slot N). Bits above the "
                    "number of slots are ignored."));

static bool isEvent(const Instruction &I) {
  const IntrinsicInst *II = dyn_cast<IntrinsicInst>(&I);
  return II && II->getIntrinsicID() == Intrinsic::amdgcn_sqtt_event;
}

// Do not move events across these kinds of instructions.
static bool isEventFence(const Instruction &I) {
  return I.mayHaveSideEffects();
}

static MDNode *getEventMetadata(const Instruction &I) {
  assert(isEvent(I));
  const IntrinsicInst *II = cast<IntrinsicInst>(&I);
  return cast<MDNode>(
      cast<MetadataAsValue>(II->getArgOperand(0))->getMetadata());
}

static void assignEventIDs(Module &M, Function &SQTTEventFun) {
  FunctionType *FT = SQTTEventFun.getFunctionType();
  Type *IdTy = FT->getParamType(1);

  NamedMDNode *EventsMD = M.getOrInsertNamedMetadata(sqtt::EventsTableMetadata);

  for (User *U : SQTTEventFun.users()) {
    IntrinsicInst *II = cast<IntrinsicInst>(U);

    Use &EventIdOperand = II->getArgOperandUse(1);
    EventIdOperand.set(ConstantInt::get(IdTy, EventsMD->getNumOperands()));
    EventsMD->addOperand(getEventMetadata(*II));
  }
}

static void
removeRedundantSetEventTableEvents(const IntrinsicInst &First,
                                   SmallVectorImpl<sqtt::Event> &Events) {
  bool IsCULocal = cast<ConstantInt>(First.getArgOperand(2))->getZExtValue();

  // We want to preserve only the last setEventTableBase event. And only if the
  // BaseOffset actually changes at the end of the event.
  auto LastSetEventTableBase =
      find_if(Events.rbegin(), Events.rend(),
              std::mem_fn(&sqtt::Event::isSetEventTableBase));
  if (LastSetEventTableBase == Events.rend())
    return;

  auto EraseEnd = LastSetEventTableBase.base() - 1;
  auto EraseBegin = remove_if(Events.begin(), EraseEnd,
                              std::mem_fn(&sqtt::Event::isSetEventTableBase));

  // The last event sets the base offset to the same value as the incoming
  // offset, we can drop it too.
  uint32_t IncomingIndex = IsCULocal ? 0 : sqtt::SectionBase;
  if (LastSetEventTableBase->getIndex() == IncomingIndex)
    ++EraseEnd;

  Events.erase(EraseBegin, EraseEnd);
}

static void combineEvents(const TargetMachine &TM, Function &SQTTEventFun) {
  LLVMContext &C = SQTTEventFun.getContext();
  MapVector<BasicBlock *, unsigned> IntrinsicBlocks;
  for (User *U : SQTTEventFun.users()) {
    IntrinsicInst *II = cast<IntrinsicInst>(U);
    ++IntrinsicBlocks[II->getParent()];
  }

  for (auto &[BB, IntrinsicCount] : IntrinsicBlocks) {
    if (IntrinsicCount < 2)
      continue;

    const TargetTransformInfo &TTI =
        TM.getTargetTransformInfo(*BB->getParent());

    Instruction *First = &*find_if(*BB, isEvent);
    SmallVector<Instruction *> ToMerge = {First};

    auto DoMerge = [&]() {
      if (ToMerge.size() < 2)
        return;

      IntrinsicInst &First = cast<IntrinsicInst>(*ToMerge.front());

      SmallVector<sqtt::Event> Events;
      Events.reserve(ToMerge.size());
      for (Instruction *I : ToMerge) {
        MDNode *MD = getEventMetadata(*I);
        Events.emplace_back(sqtt::Event::fromMetadata(MD));
      }

      removeRedundantSetEventTableEvents(First, Events);

      if (Events.empty()) {
        for_each(ToMerge, std::mem_fn(&Instruction::eraseFromParent));
        return;
      }

      sqtt::EventRecord Record(Events);
      First.setArgOperand(0, MetadataAsValue::get(C, Record.toMetadata(C)));
      for_each(drop_begin(ToMerge), std::mem_fn(&Instruction::eraseFromParent));
    };

    unsigned IntrinsicsAfterCur = IntrinsicCount - 1;
    while (IntrinsicsAfterCur > 0) {
      Instruction *LastSeen = ToMerge.back();
      Instruction *Next = &*std::find_if(LastSeen->getNextNode()->getIterator(),
                                         BB->end(), isEvent);

      InstructionCost Latency = 0;
      for (auto It = LastSeen->getNextNode()->getIterator(),
                End = Next->getIterator();
           Latency < CombineEventsThreshold && It != End; ++It) {
        const Instruction &I = *It;
        if (isEventFence(I))
          Latency = CombineEventsThreshold.getValue();
        else
          Latency +=
              TTI.getInstructionCost(&*It, TargetTransformInfo::TCK_Latency);
      }

      if (Latency < CombineEventsThreshold) {
        ToMerge.push_back(Next);
      } else {
        DoMerge();
        ToMerge = {Next};
      }
      --IntrinsicsAfterCur;
    }
    DoMerge();
  }
}

static bool canBeCalledFromOutsideTheCU(const Function &F) {
  return !F.hasLocalLinkage() ||
         F.hasAddressTaken(nullptr, /*IgnoreCallbackUses=*/true,
                           /*IgnoreAssumeLikeCalls=*/true,
                           /*IgnoreLLVMUsed=*/true);
}

static CallInst *insertSetEventTableBase(Instruction &InsertBefore,
                                         uint32_t Index, bool IsCULocal) {
  IRBuilder<> Builder(&InsertBefore);
  LLVMContext &C = Builder.getContext();
  Metadata *EventMD = sqtt::Event::setEventTableBase(Index).toMetadata(C);
  Value *Args[] = {MetadataAsValue::get(C, EventMD),
                   PoisonValue::get(Builder.getInt32Ty()),
                   Builder.getInt1(IsCULocal)};
  return Builder.CreateIntrinsicWithoutFolding(Intrinsic::amdgcn_sqtt_event,
                                               Args);
}

static CallInst *setEventTableToCUBase(Instruction &InsertBefore) {
  return insertSetEventTableBase(InsertBefore, /*Index=*/0,
                                 /*IsCULocal=*/false);
}

static CallInst *setEventTableToSectionBase(Instruction &InsertBefore) {
  return insertSetEventTableBase(InsertBefore, sqtt::SectionBase,
                                 /*IsCULocal=*/true);
}

static void insertCUBoundaryEventForFunction(Function &F) {
  BasicBlock &EntryBB = F.getEntryBlock();
  BasicBlock::iterator InsertBeforeEntry = EntryBB.getFirstNonPHIOrAlloca();
  setEventTableToCUBase(*InsertBeforeEntry);

  bool ReturnsToHost = AMDGPU::isKernel(F);
  if (ReturnsToHost)
    return;

  for (BasicBlock &BB : F) {
    ReturnInst *RI = dyn_cast<ReturnInst>(BB.getTerminator());
    if (!RI)
      continue;
    setEventTableToSectionBase(*RI);
  }
}

static void insertCUBoundaryEventForCallSite(CallInst &CI) {
  setEventTableToSectionBase(CI);
  setEventTableToCUBase(*CI.getNextNode());
}

static void insertCUBoundaryEvents(Module &M) {
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    if (!canBeCalledFromOutsideTheCU(F))
      continue;
    insertCUBoundaryEventForFunction(F);
  }

  for (Function &F : M) {
    for (Instruction &I : make_early_inc_range(instructions(F))) {
      CallInst *CI = dyn_cast<CallInst>(&I);
      if (!CI)
        continue;
      Function *CalledFunction = CI->getCalledFunction();
      if (CalledFunction && (CalledFunction->isIntrinsic() ||
                             !canBeCalledFromOutsideTheCU(*CalledFunction)))
        continue;
      insertCUBoundaryEventForCallSite(*CI);
    }
  }
}

static Value *createHWMask(IRBuilder<> &Builder, unsigned HWRegEncoding,
                           uint32_t Mask) {
  // Computes (e.g. for SIMD_ID):
  //   %simd = call i32 @llvm.amdgcn.s.getreg(<SIMD_ID field>)
  //   %mask = lshr SIMDMask, %simd  (the lowest bit contains the condition)
  auto [HwregId, Offset, Width] =
      AMDGPU::Hwreg::HwregEncoding::decode(HWRegEncoding);
  unsigned MaxHWReg = 1u << Width;
  uint32_t AllEanbledMask = maskTrailingOnes<uint32_t>(MaxHWReg);
  Mask &= AllEanbledMask;

  if (Mask == AllEanbledMask)
    return Builder.getInt32(-1);

  CallInst *SIMDId = Builder.CreateIntrinsicWithoutFolding(
      Intrinsic::amdgcn_s_getreg, {}, {Builder.getInt32(HWRegEncoding)});
  // The field is MaxHWReg values wide, tell the optimizers.
  SIMDId->addRangeRetAttr(ConstantRange(APInt(32, 0), APInt(32, MaxHWReg)));

  return Builder.CreateLShr(Builder.getInt32(Mask), SIMDId);
}

static Value *createCondition(IRBuilder<> &Builder, const GCNSubtarget &ST) {
  const std::pair<unsigned, uint32_t> Filters[] = {
      {ST.getSIMDIdHwregEncoding(), SIMDMask},
      {ST.getWaveIdHwregEncoding(), WaveMask},
      {ST.getCUIdHwregEncoding(), CUMask},
      {ST.getWGIdHwregEncoding(), WGMask},
  };

  Value *Bit = Builder.getInt32(1);
  for (const auto &[HWRegEncoding, Mask] : Filters) {
    Value *FilterBit = createHWMask(Builder, HWRegEncoding, Mask);
    ConstantInt *FilterBitConst = dyn_cast<ConstantInt>(FilterBit);
    if (FilterBitConst && FilterBitConst->isAllOnesValue())
      continue;
    Bit = Builder.CreateAnd(Bit, FilterBit);
  }

  return Builder.CreateICmpNE(Bit, Builder.getInt32(0), "sqtt.enabled");
}

static bool cannotDuplicate(const Instruction &I) {
  const CallBase *CB = dyn_cast<CallBase>(&I);
  return CB && CB->cannotDuplicate();
}

void guardBlockEventsWithFilter(Value *Condition,
                                SmallVectorImpl<Instruction *> &Events) {
  bool CanDuplicate =
      Events.size() > 1 &&
      std::none_of(Events.front()->getIterator(), Events.back()->getIterator(),
                   cannotDuplicate);
  if (!CanDuplicate) {
    for (Instruction *E : Events) {
      Instruction *ThenTerm = SplitBlockAndInsertIfThen(
          Condition, E->getIterator(), /*Unreachable=*/false);
      E->moveBefore(ThenTerm->getIterator());
    }
    return;
  }

  // Guard all the events of a basic block (in instruction order) with a single
  // branch, by versioning the code from the first to the last event:
  //
  //   head:
  //     br i1 %enabled, label %traced, label %untraced
  //   traced:     ; the original code, with the events
  //     br label %cont
  //   untraced:   ; a copy of it, without the events
  //     br label %cont
  //   cont:
  //     ; values defined in the versioned code are merged with PHIs
  //
  // This duplicates the code between the events, but only costs a single
  // branch.
  BasicBlock &BB = *Events.front()->getParent();
  Instruction *Last = Events.back();
  BasicBlock *Cont = BB.splitBasicBlock(Last->getNextNode(), "sqtt.cont");
  BasicBlock *Traced = BB.splitBasicBlock(Events.front(), "sqtt.traced");
  Function &F = *BB.getParent();

  ValueToValueMapTy VMap;
  BasicBlock *Untraced = CloneBasicBlock(Traced, VMap, ".untraced", &F);
  Traced->moveAfter(Untraced);
  Cont->moveAfter(Traced);

  for (Instruction &I : *Untraced)
    RemapInstruction(&I, VMap,
                     RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
  for (Instruction *Event : Events)
    cast<Instruction>(VMap[Event])->eraseFromParent();

  // Choose the version in the head block.
  BB.getTerminator()->eraseFromParent();
  IRBuilder<>(&BB).CreateCondBr(Condition, Traced, Untraced);

  // Merge the values defined in the versioned code and used after it. Both
  // versions fall through to the continuation block, so a PHI there is enough.
  for (Instruction &I : *Traced) {
    if (I.getType()->isVoidTy() || !I.isUsedOutsideOfBlock(Traced))
      continue;
    // The PHI has no operands yet, so it is not one of the users replaced.
    PHINode *Merge =
        PHINode::Create(I.getType(), 2, I.getName(), Cont->begin());
    I.replaceUsesWithIf(Merge, [&](Use &U) {
      return cast<Instruction>(U.getUser())->getParent() != Traced;
    });
    Merge->addIncoming(&I, Traced);
    Merge->addIncoming(VMap[&I], Untraced);
  }
}

void guardEventsWithFilter(const TargetMachine &TM, Function &Intrinsic) {
  MapVector<Function *, SetVector<BasicBlock *>> BlocksWithEvents;

  for (User *U : Intrinsic.users()) {
    IntrinsicInst *II = cast<IntrinsicInst>(U);
    BlocksWithEvents[II->getFunction()].insert(II->getParent());
  }

  for (auto [F, Blocks] : BlocksWithEvents) {
    const GCNSubtarget &ST =
        *static_cast<const GCNSubtarget *>(TM.getSubtargetImpl(*F));
    IRBuilder<> Builder(&*F->getEntryBlock().getFirstNonPHIOrAlloca());
    Value *Condition = createCondition(Builder, ST);
    if (Condition == Builder.getTrue())
      continue;

    for (BasicBlock *BB : Blocks) {
      SmallVector<Instruction *> Events;
      for (Instruction &I : *BB)
        if (isEvent(I))
          Events.push_back(&I);

      assert(!Events.empty());

      guardBlockEventsWithFilter(Condition, Events);
    }
  }
}

static bool run(Module &M, const TargetMachine &TM) {
  Function *Intrinsic =
      M.getFunction(Intrinsic::getName(Intrinsic::amdgcn_sqtt_event));

  if (!Intrinsic || Intrinsic->user_empty())
    return false;

  if (AMDGPUTargetMachine::EnableObjectLinking)
    insertCUBoundaryEvents(M);

  if (CombineEventsThreshold > 0)
    combineEvents(TM, *Intrinsic);
  assignEventIDs(M, *Intrinsic);
  guardEventsWithFilter(TM, *Intrinsic);
  return true;
}

class AMDGPUSQTTLowerLegacy : public ModulePass {
public:
  static char ID;

  AMDGPUSQTTLowerLegacy() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetPassConfig>();
  }

  bool runOnModule(Module &M) override;
};

} // namespace

bool AMDGPUSQTTLowerLegacy::runOnModule(Module &M) {
  auto &TPC = getAnalysis<TargetPassConfig>();
  return ::run(M, TPC.getTM<AMDGPUTargetMachine>());
}

PreservedAnalyses AMDGPUSQTTLowerPass::run(Module &M, ModuleAnalysisManager &) {
  if (!::run(M, TM))
    return PreservedAnalyses::all();
  return PreservedAnalyses::none();
}

INITIALIZE_PASS_BEGIN(AMDGPUSQTTLowerLegacy, DEBUG_TYPE, "AMDGPU SQTT lower",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_END(AMDGPUSQTTLowerLegacy, DEBUG_TYPE, "AMDGPU SQTT lower",
                    false, false)

char AMDGPUSQTTLowerLegacy::ID = 0;

ModulePass *llvm::createAMDGPUSQTTLowerLegacyPass() {
  return new AMDGPUSQTTLowerLegacy;
}
