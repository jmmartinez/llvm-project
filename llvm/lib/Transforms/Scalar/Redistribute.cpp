//===- Redistribute.cpp - Redistribute k*(a+...b) expressions - C++ -*-=======//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Comments in this pass are written in terms of the expression (a+b)*k.
// * The "+" operation is not necessarily an addition, it could be a disjoint
// or, or a sub.
// * The "*" operation is not necessarily a multiplication, it could be a shift
// operation (WIP).
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar/Redistribute.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"

#define DEBUG_TYPE "redistribute"

using namespace llvm;

namespace {
using BinaryOps = Instruction::BinaryOps;

struct DistributedOp {
  Value *Operand;
  Value *K;
  BinaryOps Opcode;
};
} // namespace

namespace llvm {
template <> struct DenseMapInfo<DistributedOp> {
  static inline DistributedOp getEmptyKey() {
    Value *Empty = DenseMapInfo<Value *>::getEmptyKey();
    return {Empty, Empty, BinaryOps::BinaryOpsEnd};
  }

  static inline DistributedOp getTombstoneKey() {
    Value *Tomb = DenseMapInfo<Value *>::getTombstoneKey();
    return {Tomb, Tomb, BinaryOps::BinaryOpsEnd};
  }

  static unsigned getHashValue(const DistributedOp &Op) {
    unsigned Operand = DenseMapInfo<Value *>::getHashValue(Op.Operand);
    unsigned K = DenseMapInfo<Value *>::getHashValue(Op.K);
    unsigned Opcode =
        DenseMapInfo<unsigned>::getHashValue(static_cast<unsigned>(Op.Opcode));
    return detail::combineHashValue(detail::combineHashValue(Operand, K),
                                    Opcode);
  }

  static bool isEqual(const DistributedOp &A, const DistributedOp &B) {
    return A.Operand == B.Operand && A.K == B.K && A.Opcode == B.Opcode;
  }
};
} // namespace llvm

namespace {
static bool canDistributeInst(const Instruction &I) {
  switch (I.getOpcode()) {
  case BinaryOps::Mul:
    return I.getOperand(0) != I.getOperand(1);
  default:
    break;
  }
  return false;
}

static bool canDistributeInstOver(const BinaryOperator &Over) {
  return Over.getOpcode() == Instruction::Add ||
         Over.getOpcode() == Instruction::Sub;
}

static Value *createDistributedBinOp(IRBuilder<> &B, BinaryOps ToDistribute,
                                     BinaryOps Over, Value *LHS, Value *RHS) {
  switch (ToDistribute) {
  case BinaryOps::Mul: {
    assert(is_contained({BinaryOps::Add, BinaryOps::Sub}, Over));
    return B.CreateBinOp(Over, LHS, RHS);
  }
  default:
    break;
  }
  llvm_unreachable("Unexpected opcode to distribute over binary operator.");
}

static BasicBlock::iterator getNextInsertPt(Instruction *I) {
  BasicBlock::iterator It = I->getIterator();
  do {
    if (InvokeInst *II = dyn_cast<InvokeInst>(I)) {
      It = II->getNormalDest()->begin();
      continue;
    }
    ++It;
  } while (isa<PHINode>(*It));
  return It;
}

static Instruction *getDominatedInst(DominatorTree &DT, Instruction &I1,
                                     Instruction &I2) {
  BasicBlock *BB1 = I1.getParent();
  BasicBlock *BB2 = I2.getParent();
  if (BB1 == BB2)
    return I1.comesBefore(&I2) ? &I2 : &I1;
  return DT.dominates(BB1, BB2) ? &I2 : &I1;
}

static BasicBlock::iterator findInsertPoint(DominatorTree &DT,
                                            ArrayRef<Value *> Vals) {

  Instruction *IP = nullptr;
  for (Value *V : Vals) {
    Instruction *I = dyn_cast<Instruction>(V);
    if (!I)
      continue;
    if (!IP) {
      IP = I;
      continue;
    }
    IP = getDominatedInst(DT, *IP, *I);
  }

  if (IP)
    return getNextInsertPt(IP);
  return DT.getRoot()->getFirstNonPHIOrDbgOrAlloca();
}

static bool wouldReuseOtherDistributedOperands(
    const BinaryOperator &ToDistribute, const Value &Over,
    const SmallPtrSetImpl<Value *> &OtherValuesForWhichTheSameOpIsApplied) {
  const BinaryOperator *OverBinOp = dyn_cast<BinaryOperator>(&Over);
  if (!OverBinOp)
    return false;
  if (!canDistributeInstOver(*OverBinOp))
    return false;
  auto IsInOtherValuesForWhichTheSameOpIsApplied = [&](const Value *Op) {
    return OtherValuesForWhichTheSameOpIsApplied.contains(Op);
  };
  return any_of(OverBinOp->operands(),
                IsInOtherValuesForWhichTheSameOpIsApplied);
}

using UseVector = SmallVector<Use *, 8>;

static void collectAllUsesToDistribute(Function &F,
                                       SmallVectorImpl<Use *> &Uses) {
  for (Instruction &I : instructions(F)) {
    if (!canDistributeInst(I))
      continue;

    unsigned N = I.isCommutative() ? 2 : 1;
    for (unsigned Op = 0; Op != N; ++Op)
      Uses.push_back(&I.getOperandUse(Op));
  }
}

static Value *getTheOtherOperand(Use &U) {
  User *BinOp = U.getUser();
  assert(isa<BinaryOperator>(BinOp));
  return BinOp->getOperand((U.getOperandNo() + 1) % 2);
}

static void
keepUsesThatReuseDistributedOperands(SmallVectorImpl<Use *> &AllUses) {
  using GroupBy = std::pair<unsigned, Value *>;

  // Group instructions by opcode and operand such that all (a+b...+c)*k are
  // together.
  DenseMap<GroupBy, UseVector> GroupedUses;
  for (Use *U : AllUses) {
    BinaryOperator *ToDistribute =
        cast<BinaryOperator>(U->getUser()); // in (a+b)*k, this is *

    BinaryOps Opcode = ToDistribute->getOpcode();
    Value *K = getTheOtherOperand(*U);

    UseVector &Over = GroupedUses.try_emplace({Opcode, K}).first->second;
    Over.push_back(U);
  }

  AllUses.clear();

  for (const auto &[_, Uses] : GroupedUses) {
    if (Uses.size() < 2)
      continue;

    // Contains all the values over which *k is applied
    SmallPtrSet<Value *, 8> UseValues;
    for (Use *U : Uses)
      UseValues.insert(U->get());

    // Filter uses that do not use another value over which the operation to
    // distribute is applied. For (a+b)*k, the first operand use is kept only if
    // a*k or b*k also exists.
    UseVector UsesToDistribute;
    for (Use *U : Uses) {
      BinaryOperator *ToDistribute = cast<BinaryOperator>(U->getUser());
      Value *Over = U->get();
      if (!wouldReuseOtherDistributedOperands(*ToDistribute, *Over, UseValues))
        continue;
      UsesToDistribute.push_back(U);
    }

    // Compare the number of instructions before and after the pass.
    // The number of '+' operations remains the same.
    // The number of '*' operations depends on how many new operands that were
    // not already calculated in the original code we have to distribute.
    SmallPtrSet<Value *, 8> NewOperandsNotOriginallyDistributed;
    for (Use *U : UsesToDistribute) {
      BinaryOperator *Over = cast<BinaryOperator>(U->get());
      for (unsigned OpIdx = 0; OpIdx != 2; ++OpIdx) {
        Value *Op = Over->getOperand(OpIdx);
        if (UseValues.contains(Op))
          continue;
        NewOperandsNotOriginallyDistributed.insert(Op);
      }
    }

    // This condition is restrictive.
    // Could be refined to use the TargetTransformInfo to determine when
    // combinations opcodes can be fused (e.g. multiply-and-add).
    const size_t NewDistrOpCount = NewOperandsNotOriginallyDistributed.size();
    const size_t OriginalDistrOpCount = UsesToDistribute.size();
    bool IsProfitable = NewDistrOpCount < OriginalDistrOpCount;
    if (IsProfitable)
      AllUses.append(UsesToDistribute);
  }
}

static void keepUsesThatDoNotHaveUsesOutside(SmallVectorImpl<Use *> &AllUses) {
  bool Changed;
  do {
    Changed = false;
    DenseMap<Value *, SmallPtrSet<Use *, 2>> UsesOfOver;
    for (Use *U : AllUses) {
      Value *Over = U->get();
      UsesOfOver[Over].insert(U);
    }

    AllUses.clear();

    // We allow (a+b)*k to be distributed if there are no other uses of (a+b)
    // outside:
    // * another operation (a+b)*q that will be distributed too,
    // * or other candidates to distribute for the same operation, such as
    //   ((a+b)+c)*k.
    for (const auto &[Over, UsesOfOverToDistribute] : UsesOfOver) {
      auto const &UsesOfOverToDistributeRef =
          UsesOfOverToDistribute; // Assign structured binding to a local to
                                  // avoid C++20 extension warning.
      bool AllUsesAreToDistribute = all_of(Over->uses(), [&](Use &UseOfOver) {
        return UsesOfOver.contains(UseOfOver.getUser()) ||
               UsesOfOverToDistributeRef.contains(&UseOfOver);
      });
      if (!AllUsesAreToDistribute) {
        Changed = true;
        continue;
      }
      append_range(AllUses, UsesOfOverToDistribute);
    }
  } while (Changed);
}

static void sortByExecutionOrder(DominatorTree &DT,
                                 SmallVectorImpl<Use *> &AllCandidateUses) {
  SmallPtrSet<Value *, 32> Over;
  for (Use *U : AllCandidateUses) {
    Over.insert(U->get());
    Over.insert(U->getUser());
  }

  // We want to iterate over the users in the order in
  // which their operands appear when we generate code for the transformation.
  DenseMap<Value *, unsigned> ExecutionOrder;

  for (auto *Node : depth_first(&DT)) {
    BasicBlock &BB = *Node->getBlock();
    for (Instruction &I : BB) {
      if (Over.contains(&I))
        ExecutionOrder[&I] = ExecutionOrder.size();
    }
  }

  auto SortByExecuteBefore = [&](const Use *A, const Use *B) {
    unsigned OrderOverA = ExecutionOrder[A->get()];
    unsigned OrderOverB = ExecutionOrder[B->get()];
    if (OrderOverA < OrderOverB)
      return true;
    if (OrderOverA > OrderOverB)
      return false;
    unsigned OrderUserA = ExecutionOrder[A->getUser()];
    unsigned OrderUserB = ExecutionOrder[B->getUser()];
    return OrderUserA < OrderUserB;
  };
  stable_sort(AllCandidateUses, SortByExecuteBefore);
}

static void collectCandidates(Function &F, DominatorTree &DT,
                              SmallVectorImpl<Use *> &AllCandidateUses) {

  collectAllUsesToDistribute(F, AllCandidateUses);
  keepUsesThatReuseDistributedOperands(AllCandidateUses);
  keepUsesThatDoNotHaveUsesOutside(AllCandidateUses);
  sortByExecutionOrder(DT, AllCandidateUses);

  LLVM_DEBUG({
    for (Use *U : AllCandidateUses) {
      dbgs() << "Redistribute: On " << F.getName()
             << ". Found candidate operation to distribute: " << *U->getUser()
             << " over " << *U->get() << "\n";
    }
  });
}

static void
redistributeCandidates(DominatorTree &DT, SmallVectorImpl<Use *> &Candidates,
                       DenseMap<DistributedOp, Value *> &AlreadyDistributed) {
  assert(!Candidates.empty());

  LLVMContext &C = Candidates.front()->get()->getContext();
  IRBuilder<> B(C);

  DenseMap<Value *, Value *> FrozenK;

  SetVector<BinaryOperator *> OverToErase;
  for (Use *U : Candidates) {
    Value *K = getTheOtherOperand(*U);
    BinaryOperator *ToDistribute = cast<BinaryOperator>(U->getUser());
    BinaryOps ToDistributeOpcode = ToDistribute->getOpcode();
    BinaryOperator *Over = cast<BinaryOperator>(U->get());
    BinaryOps OverOpcode = Over->getOpcode();

    auto [It, Inserted] =
        AlreadyDistributed.try_emplace({Over, K, ToDistributeOpcode}, nullptr);
    if (!Inserted) {
      Value *Distributed = It->second;
      LLVM_DEBUG(dbgs() << "Redistribute: Replacing " << *ToDistribute
                        << " with: " << *Distributed << "\n");
      ToDistribute->replaceAllUsesWith(Distributed);
      ToDistribute->eraseFromParent();
      continue;
    }

    // If rhs can be undef, we have to freeze it to preserve the semantics.
    auto [FrozenKEntry, FrozenKInserted] = FrozenK.try_emplace(K, K);
    if (FrozenKInserted) {
      if (!isGuaranteedNotToBeUndef(K, nullptr, nullptr, &DT)) {
        B.SetInsertPoint(findInsertPoint(DT, {K}));
        FrozenKEntry->second = B.CreateFreeze(K, K->getName() + ".freeze");
        LLVM_DEBUG(dbgs() << "Redistribute: Create freeze for " << *K << ": "
                          << *FrozenKEntry->second << "\n");
      }
    }
    Value *FrozenK = FrozenKEntry->second;

    // For lhs=a+b, get a*k and b*k, then create the distributed operation.
    Value *NewDistributedOperands[2];
    for (unsigned OpIdx = 0; OpIdx != 2; ++OpIdx) {
      Value *Op = Over->getOperand(OpIdx);
      auto [Iterator, Inserted] =
          AlreadyDistributed.try_emplace({Op, K, ToDistributeOpcode}, nullptr);
      if (Inserted) {
        B.SetInsertPoint(findInsertPoint(DT, {Op, FrozenK}));
        Iterator->second = B.CreateBinOp(ToDistributeOpcode, Op, FrozenK,
                                         Op->getName() + ".distr.op");
        LLVM_DEBUG(dbgs() << "Redistribute: Adding distributed operand " << *Op
                          << " for " << *ToDistribute << ": "
                          << Iterator->second << "\n");
      }
      NewDistributedOperands[OpIdx] = Iterator->second;
    }

    // Insert in the highest point possible, such that any other use of lhs*k
    // by another instruction to distribute is dominated.
    B.SetInsertPoint(
        findInsertPoint(DT, {Over, FrozenK, NewDistributedOperands[0],
                             NewDistributedOperands[1]}));
    Value *Distributed = createDistributedBinOp(
        B, ToDistributeOpcode, OverOpcode, NewDistributedOperands[0],
        NewDistributedOperands[1]);

    LLVM_DEBUG(dbgs() << "Redistribute: Replacing " << *ToDistribute
                      << " with: " << *Distributed << "\n");

    AlreadyDistributed[{Over, K, ToDistributeOpcode}] =
        Distributed; // Do not reuse `It`, it may be invalid now.
    ToDistribute->replaceAllUsesWith(Distributed);
    Distributed->takeName(ToDistribute);
    ToDistribute->eraseFromParent();

    // Delay deleting 'a+b' since it is likely still used by '(a+b)+b'.
    OverToErase.insert(Over);
  }

  // Delete the lhs in reverse order in case one lhs uses another as operand.
  while (!OverToErase.empty()) {
    BinaryOperator *Over = OverToErase.pop_back_val();
    LLVM_DEBUG(dbgs() << "Redistribute: Removing " << *Over << "\n");
    assert(Over->user_empty());
    Over->eraseFromParent();
  }
}

static void
removeRedundantOperands(Function &F,
                        const DenseMap<DistributedOp, Value *> &Distributed) {
  // Scan again the instructions looking for operands that were distributed and
  // that are now redundant.
  //
  // For:
  //   f(a*k)
  //   f((a+b)*k)
  //
  // We now have:
  //   ak = a*freeze(k)
  //   bk = b*freeze(k)
  //   f(a*k)
  //   f(ak + bk)
  //
  // We can replace a*k with ak, even if ak is "less undefined" than a*k.

  for (Instruction &I : make_early_inc_range(instructions(F))) {
    if (!canDistributeInst(I))
      continue;

    BinaryOperator &B = cast<BinaryOperator>(I);
    unsigned N = B.isCommutative() ? 2 : 1;
    for (unsigned i = 0; i != N; ++i) {
      Value *Op = B.getOperand(i);
      Value *K = B.getOperand((i + 1) % 2);
      auto It = Distributed.find(DistributedOp{Op, K, B.getOpcode()});
      if (It == Distributed.end())
        continue;
      Value *ReplaceWith = It->second;
      if (ReplaceWith == &B)
        break;

      ReplaceWith->takeName(&B);
      LLVM_DEBUG(dbgs() << "Redistribute: Replacing " << B << " with "
                        << *ReplaceWith << "\n");
      B.replaceAllUsesWith(ReplaceWith);
      B.eraseFromParent();
      break;
    }
  }
}

static bool runImpl(Function &F, DominatorTree &DT) {
  UseVector Candidates;
  collectCandidates(F, DT, Candidates);
  if (Candidates.empty())
    return false;

  // group distributed operands by Over, K, Opcode
  DenseMap<DistributedOp, Value *> Distributed;
  redistributeCandidates(DT, Candidates, Distributed);

  removeRedundantOperands(F, Distributed);

  return true;
}

class RedistributeLegacyPass : public FunctionPass {
public:
  static char ID;

  RedistributeLegacyPass() : FunctionPass(ID) {
    initializeRedistributeLegacyPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override {
    if (skipFunction(F))
      return false;

    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    return runImpl(F, DT);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.setPreservesCFG();
  }
};

char RedistributeLegacyPass::ID = 0;
} // namespace

PreservedAnalyses RedistributePass::run(Function &F,
                                        FunctionAnalysisManager &AM) {
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  if (!runImpl(F, DT))
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

INITIALIZE_PASS_BEGIN(RedistributeLegacyPass, "redistribute", "Redistribute",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(RedistributeLegacyPass, "redistribute", "Redistribute",
                    false, false)

FunctionPass *llvm::createRedistributePass() {
  return new RedistributeLegacyPass();
}
