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
#include "llvm/Frontend/SQTT/Event.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "amdgpu-sqtt-lower"

using namespace llvm;

namespace {

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

    MetadataAsValue *EventMD = cast<MetadataAsValue>(II->getArgOperand(0));
    EventsMD->addOperand(cast<MDNode>(EventMD->getMetadata()));
  }
}

static bool run(Module &M) {
  Function *Intrinsic =
      M.getFunction(Intrinsic::getName(Intrinsic::amdgcn_sqtt_event));
  if (!Intrinsic)
    return false;
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
