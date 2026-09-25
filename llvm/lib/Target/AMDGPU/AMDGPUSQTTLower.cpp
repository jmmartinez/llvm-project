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
#include "llvm/ADT/STLExtras.h"
#include "llvm/Frontend/SQTT/Event.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"

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

static void combineEvents(Module &M, Function &SQTTEventFun) {
  LLVMContext &C = M.getContext();
  MapVector<BasicBlock *, SmallVector<IntrinsicInst *, 4>> IntrinsicBlocks;
  Type *IdTy = SQTTEventFun.getFunctionType()->getParamType(1);

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

      Metadata *MergedEventMD = sqtt::MergedEvent{Events}.toMetadata(C);
      SmallVector<Value *> Args{MetadataAsValue::get(C, MergedEventMD),
                                PoisonValue::get(IdTy)};
      for (IntrinsicInst *II : Range)
        append_range(Args, drop_begin(II->args(), 2));

      IntrinsicInst *First = (*std::begin(Range));
      IRBuilder<> B(First);
      B.CreateCall(&SQTTEventFun, Args);

      for_each(make_early_inc_range(Range),
               std::mem_fn(&IntrinsicInst::eraseFromParent));
    }
  }
}

static bool run(Module &M) {
  Function *Intrinsic =
      M.getFunction(Intrinsic::getName(Intrinsic::amdgcn_sqtt_event));
  if (!Intrinsic)
    return false;
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
