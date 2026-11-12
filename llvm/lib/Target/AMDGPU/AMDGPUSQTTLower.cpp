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
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/AMDGPU/SQTTEvent.h"

#define DEBUG_TYPE "amdgpu-sqtt-lower"

using namespace llvm;

namespace {
static cl::opt<bool> CombineEvents("amdgpu-sqtt-combine-events", cl::init(true),
                                   cl::Hidden,
                                   cl::desc("Combine consecutive SQTT events"));

static bool isEvent(const Instruction &I) {
  const IntrinsicInst *II = dyn_cast<IntrinsicInst>(&I);
  return II && II->getIntrinsicID() == Intrinsic::amdgcn_sqtt_event;
}

static MDNode *getEventMetadata(IntrinsicInst *II) {
  assert(isEvent(*II));
  return cast<MDNode>(
      cast<MetadataAsValue>(II->getArgOperand(0))->getMetadata());
}

static void assignEventIDs(Module &M, Function &SQTTEventFun) {
  FunctionType *FT = SQTTEventFun.getFunctionType();
  Type *IdTy = FT->getParamType(1);

  NamedMDNode *EventsMD = M.getOrInsertNamedMetadata(sqtt::EventsTableMetadata);

  // This assumes a closed world. This will not work with multiple compilation
  // units.
  for (User *U : SQTTEventFun.users()) {
    IntrinsicInst *II = cast<IntrinsicInst>(U);

    Use &EventIdOperand = II->getArgOperandUse(1);
    EventIdOperand.set(ConstantInt::get(IdTy, EventsMD->getNumOperands()));
    EventsMD->addOperand(getEventMetadata(II));
  }
}

static auto findEventRange(Instruction &I) {
  auto FirstNotEvent =
      std::find_if_not(I.getReverseIterator(), I.getParent()->rend(), isEvent);
  auto FirstEvent = std::prev(FirstNotEvent);
  auto FirstEventIt = FirstEvent->getIterator();
  auto End = std::find_if_not(I.getIterator(), I.getParent()->end(), isEvent);
  auto AsIntrinsic = [](Instruction &I) -> IntrinsicInst * {
    return cast<IntrinsicInst>(&I);
  };
  return map_range(make_range(FirstEventIt, End), AsIntrinsic);
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

static void combineEvents(Module &M, Function &SQTTEventFun) {
  LLVMContext &C = M.getContext();
  MapVector<BasicBlock *, SmallVector<IntrinsicInst *, 4>> IntrinsicBlocks;
  for (User *U : SQTTEventFun.users()) {
    IntrinsicInst *II = cast<IntrinsicInst>(U);
    IntrinsicBlocks[II->getParent()].push_back(II);
  }

  for (auto &[BB, Intrinsics] : IntrinsicBlocks) {
    if (Intrinsics.size() < 2)
      continue;

    SmallPtrSet<IntrinsicInst *, 4> Seen;
    for (IntrinsicInst *II : Intrinsics) {
      if (!Seen.insert(II).second)
        continue;

      auto Range = findEventRange(*II);
      if (std::distance(std::begin(Range), std::end(Range)) < 2)
        continue;
      Seen.insert_range(Range);

      SmallVector<Metadata *> EventMDs{map_range(Range, getEventMetadata)};
      SmallVector<sqtt::Event> Events{
          map_range(EventMDs, sqtt::Event::fromMetadata)};

      IntrinsicInst *First = (*std::begin(Range));
      removeRedundantSetEventTableEvents(*First, Events);

      auto ToRemove = drop_begin(Range);
      for_each(make_early_inc_range(ToRemove),
               std::mem_fn(&IntrinsicInst::eraseFromParent));

      if (!Events.empty()) {
        Metadata *RecordMD = sqtt::EventRecord{Events}.toMetadata(C);
        First->setArgOperand(0, MetadataAsValue::get(C, RecordMD));
      } else {
        First->eraseFromParent();
      }
    }
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

static bool run(Module &M) {
  Function *Intrinsic =
      M.getFunction(Intrinsic::getName(Intrinsic::amdgcn_sqtt_event));

  if (!Intrinsic || Intrinsic->user_empty())
    return false;

  if (AMDGPUTargetMachine::EnableObjectLinking)
    insertCUBoundaryEvents(M);

  if (CombineEvents)
    combineEvents(M, *Intrinsic);
  assignEventIDs(M, *Intrinsic);
  return true;
}

class AMDGPUSQTTLowerLegacy : public ModulePass {
public:
  static char ID;

  AMDGPUSQTTLowerLegacy() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
  }

  bool runOnModule(Module &M) override;
};

} // namespace

bool AMDGPUSQTTLowerLegacy::runOnModule(Module &M) { return ::run(M); }

PreservedAnalyses AMDGPUSQTTLowerPass::run(Module &M, ModuleAnalysisManager &) {
  if (!::run(M))
    return PreservedAnalyses::all();
  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

INITIALIZE_PASS(AMDGPUSQTTLowerLegacy, DEBUG_TYPE, "AMDGPU SQTT lower", false,
                false)

char AMDGPUSQTTLowerLegacy::ID = 0;

ModulePass *llvm::createAMDGPUSQTTLowerLegacyPass() {
  return new AMDGPUSQTTLowerLegacy;
}
