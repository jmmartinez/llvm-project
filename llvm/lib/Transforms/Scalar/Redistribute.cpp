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
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"

#define DEBUG_TYPE "redistribute"

using namespace llvm;

static cl::opt<bool>
    ForceProfitable("redistribute-force-profitable", cl::init(false),
                    cl::desc("Ignore the cost model of redistribute and apply "
                             "the transformation anyways."),
                    cl::Hidden);

namespace {
using BinaryOps = Instruction::BinaryOps;

// This class keeps track of the instruction created or deleted by this
// transformation, but where the modification has not been accepted yet. In some
// platforms it is profitable to fold some instructions, like add(mul(a, b) c)
// -> mad(a, b, c). To estimate if the transformation is profitable, we have to
// compare both instruction sequences.
class Sandbox {
  SmallVector<Instruction *> NewInst;
  MapVector<Instruction *, Value *> Replacements;

  using GetInstCostType = function_ref<InstructionCost(const Instruction &)>;
  GetInstCostType GetInstructionCost;

  template <typename InstRange>
  InstructionCost accumulateInstCost(InstRange &&Insts) const {
    InstructionCost Total{0U};
    for (const Instruction *I : Insts) {
      Total += GetInstructionCost(*I);
    }
    return Total;
  }

public:
  Sandbox(GetInstCostType Callback) : GetInstructionCost(Callback) {}

  using IRBuilder = llvm::IRBuilder<ConstantFolder, IRBuilderCallbackInserter>;

  IRBuilder getIRBuilder(LLVMContext &C) {
    return IRBuilder(C, ConstantFolder(),
                     IRBuilderCallbackInserter(
                         [&](Instruction *I) { NewInst.push_back(I); }));
  }

  void replaceAndErase(Instruction *I, Value *V) { Replacements[I] = V; }

  void erase(Instruction *I) { Replacements[I] = nullptr; }

  InstructionCost getCostBefore() const {
    auto ReplacementKeys =
        map_range(Replacements, [](const auto &Pair) { return Pair.first; });
    return accumulateInstCost(ReplacementKeys);
  }

  InstructionCost getCostAfter() const { return accumulateInstCost(NewInst); }

  static void accept(Sandbox &&S) {
    for (auto [Old, New] : S.Replacements) {
      if (New) {
        Old->replaceAllUsesWith(New);
        if (!New->hasName())
          New->takeName(Old);
        LLVM_DEBUG(dbgs() << "Redistribute: Replacing " << *Old
                          << " with: " << *New << "\n");
      } else {
        LLVM_DEBUG(dbgs() << "Redistribute: Removing " << *Old << "\n");
        Old->replaceAllUsesWith(UndefValue::get(Old->getType()));
      }
      Old->eraseFromParent();
    }
  }

  static void reject(Sandbox &&S) {
    for (Instruction *New : S.NewInst) {
      New->replaceAllUsesWith(UndefValue::get(New->getType()));
      New->eraseFromParent();
    }
  }
};

static Value *createDistributedBinOp(Sandbox::IRBuilder &B,
                                     BinaryOps ToDistribute, BinaryOps Over,
                                     Value *LHS, Value *RHS) {
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

// This class represents the (a+b)*k relationship, where the * is the operation
// to distribute and + is the operation over which the distribution is done. `a`
// and `b` are the terms.
class CandidateOp {
  Use *U;

  CandidateOp(Use &U) : U(&U) {}

public:
  bool operator!=(const CandidateOp &Other) const { return U != Other.U; }

  BinaryOperator *getUser() { return cast<BinaryOperator>(U->getUser()); }

  const BinaryOperator *getUser() const {
    return cast<BinaryOperator>(U->getUser());
  }

  BinaryOperator *getOver() { return cast<BinaryOperator>(U->get()); }

  const BinaryOperator *getOver() const {
    return cast<BinaryOperator>(U->get());
  }

  Value *getValueToDistribute() {
    return U->getUser()->getOperand((U->getOperandNo() + 1) % 2);
  }

  auto getTerms() { return getOver()->operands(); }

  auto getTerms() const { return getOver()->operands(); }

  static bool canDistributeInst(const Instruction &I) {
    switch (I.getOpcode()) {
    // TODO: Add shl.
    case BinaryOps::Mul:
      return I.getOperand(0) != I.getOperand(1);
    default:
      break;
    }
    return false;
  }

  static bool canDistributeInstOver(const Use &Over) {
    BinaryOperator &I = cast<BinaryOperator>(*Over.getUser());
    Instruction *OverInst = dyn_cast<Instruction>(Over.get());
    if (!OverInst)
      return false;

    unsigned OverOp = OverInst->getOpcode();
    switch (I.getOpcode()) {
    // TODO: Add disjoint or
    case BinaryOps::Mul:
      return OverOp == Instruction::Add || OverOp == Instruction::Sub;
    default:
      break;
    }
    llvm_unreachable("Unhandled instruction opcode in switch");
  }

  static std::optional<CandidateOp> tryToGetCandidate(Use &U) {
    if (!canDistributeInstOver(U))
      return std::nullopt;
    return {CandidateOp{U}};
  }
};

class RedistributeCandidates {
  SmallVector<CandidateOp> Candidates;

  // All uses that participate in similar operations, even if not distributed
  DenseMap<Value*, SmallVector<Use*, 1>> All;

  DominatorTree *DT;

public:
  RedistributeCandidates(DominatorTree &DT) : DT(&DT) {}

  RedistributeCandidates(RedistributeCandidates &&) = default;
  RedistributeCandidates &operator=(RedistributeCandidates &&) = default;

  void add(Use &Over) {
    if (auto C = CandidateOp::tryToGetCandidate(Over))
      Candidates.emplace_back(*C);
    All[Over.get()].push_back(&Over);
  }

  size_t size() const { return Candidates.size(); }

  bool empty() const { return Candidates.empty(); }

private:
  bool reusesTerm(const CandidateOp &C) const {
    // Check that for (a+b)*k, a*k or b*k is also computed
    for (Value *Term : C.getTerms()) {
      auto It = All.find(Term);
      if(It == All.end())
        continue;

      for (Use *Other : It->second) {
        // Not any a*k works, it must be in a path such that (a+b)*k is also
        // executed.
        // This condition is too restrictive, it doesn't consider that after
        // executing (a+b)*k we may always be executing a*k. However, this
        // simplifies the algorithm.
        // TODO: if a*k is post-dominated by (a+b)*k, we can hoist a*k; this
        // requires generating code in the order in which the `over`
        // instructions appear. This risks increasing the register-pressure.
        if (DT->dominates(Other->getUser(), C.getUser()))
          return true;
      }
    }
    return false;
  }

  BinaryOperator* getTermReuse(const CandidateOp &C, Value* Term) const {
    auto It = All.find(Term);
    if(It == All.end())
      return nullptr;

    for (Use *Other : It->second)
      if (DT->dominates(Other->getUser(), C.getUser()))
        return cast<BinaryOperator>(Other->getUser());

    return nullptr;
  }

  bool canEraseOverAfterDistribution(const CandidateOp &C) const {
    // For candidate (a+b)*k, we want to ensure that (a+b) can be removed after
    // it gets transformed into a*k + b*k. Reject the candidate if there is an
    // user that is not:
    // * Another equivalent candidate (a+b)*k
    // * Another candidate ((a+b)+c)*k
    // This condition misses other candidates (a+b)*q that could also be
    // profitable if both *k and *q are distributed.
    const BinaryOperator *Over = C.getOver();
    for (const User *U : Over->users()) {
      if (U == C.getUser())
        continue;
      auto OtherCandidates = make_filter_range(
          Candidates, [&C](const CandidateOp &Other) { return Other != C; });
      auto IsEquivalentOrUsedAsOtherOverTerm = [U](const CandidateOp &Other) {
        return Other.getOver() == U || is_contained(Other.getTerms(), U);
      };
      if (!any_of(OtherCandidates, IsEquivalentOrUsedAsOtherOverTerm))
        return false;
    }
    return true;
  }

public:
  void removeTriviallyUnprofitableCandidates() {
    bool Changed;
    do {
      Changed = false;
      auto It = Candidates.begin();
      while (It != Candidates.end()) {
        if (!reusesTerm(*It) || !canEraseOverAfterDistribution(*It)) {
          It = Candidates.erase(It);
          Changed = true;
          continue;
        }
        ++It;
      }
    } while (Changed);
  }

public:
  bool tryToRedistribute(const TargetTransformInfo &TTI) {
    LLVMContext &C = DT->getRoot()->getContext();

    DenseMap<std::pair<BasicBlock *, Value *>, Value *> AlreadyDistributed;

    auto GetInstCost = [&TTI](const Instruction &I) {
      return TTI.getInstructionCost(&I,
                                    TargetTransformInfo::TCK_SizeAndLatency);
    };
    Sandbox S(GetInstCost);
    Sandbox::IRBuilder B = S.getIRBuilder(C);

    Value *K = Candidates.front().getValueToDistribute();
    // If rhs can be undef, we have to freeze it to preserve the semantics.
    if (!isGuaranteedNotToBeUndef(K, nullptr, nullptr, DT)) {
      if (Instruction *KI = dyn_cast<Instruction>(K)) {
        B.SetInsertPoint(*KI->getInsertionPointAfterDef());
      } else {
        B.SetInsertPointPastAllocas(DT->getRoot()->getParent());
      }
      K = B.CreateFreeze(K, K->getName() + ".freeze");
    }

    for (CandidateOp &C : Candidates) {
      BinaryOperator *ToDistribute = C.getUser();
      BinaryOps ToDistributeOpcode = ToDistribute->getOpcode();
      BinaryOperator *Over = C.getOver();
      BinaryOps OverOpcode = Over->getOpcode();

      // For lhs=a+b, get a*k and b*k, then create the distributed operation.
      Value *NewDistributedOperands[2];
      for (unsigned OpIdx = 0; OpIdx != 2; ++OpIdx) {
        Value *Op = Over->getOperand(OpIdx);
        auto Iterator = AlreadyDistributed.find({Over->getParent(), Op});
        if (Iterator == AlreadyDistributed.end()) {
          BinaryOperator *Reuse = getTermReuse(C, Op);
          Instruction *InsertPoint = Reuse ? Reuse : ToDistribute;

          B.SetInsertPoint(InsertPoint);
          // We pick a a*k that hasn't been distributed (one of the base
          // values). We have to recompute it using the frozen K
          Value *OpK = B.CreateBinOp(ToDistributeOpcode, Op, K);

          SmallVector<BasicBlock *> Descendants;
          DT->getDescendants(InsertPoint->getParent(), Descendants);
          for (BasicBlock *Descendant : Descendants)
            AlreadyDistributed[{Descendant, Op}] = OpK;

          if (Reuse) {
            S.replaceAndErase(Reuse, OpK);
          } else {
            OpK->setName(Op->getName() + "." + K->getName());
          }
          NewDistributedOperands[OpIdx] = OpK;
        } else {
          NewDistributedOperands[OpIdx] = Iterator->second;
        }
      }

      B.SetInsertPoint(ToDistribute);
      Value *Distributed = createDistributedBinOp(
          B, ToDistributeOpcode, OverOpcode, NewDistributedOperands[0],
          NewDistributedOperands[1]);

      SmallVector<BasicBlock *> Descendants;
      DT->getDescendants(ToDistribute->getParent(), Descendants);
      for (BasicBlock *Descendant : Descendants)
        AlreadyDistributed[{Descendant, Over}] = Distributed;

      S.replaceAndErase(ToDistribute, Distributed);
      S.erase(Over);
    }

    const InstructionCost Before = S.getCostBefore();
    const InstructionCost After = S.getCostAfter();
    assert(Before.isValid() && After.isValid());

    LLVM_DEBUG(
        dbgs() << "Redistribute: Cost before vs after the transformation "
                  "(lower is better): "
               << Before << " vs " << After << "\n");

    bool IsProfitable = ForceProfitable || After < Before;
    if (IsProfitable) {
      Sandbox::accept(std::move(S));
      return true;
    }

    Sandbox::reject(std::move(S));
    return false;
  }
};

static SmallVector<RedistributeCandidates>
collectInitialRedistributeCandidates(Function &F, DominatorTree &DT) {
  // Group instructions by opcode and operand such that all ((a+b)+...)*k are
  // grouped together using {`*`, `k`} for the key.
  using GroupBy = std::pair<unsigned, Value *>;
  DenseMap<GroupBy, RedistributeCandidates> Grouped;

  for (auto *Node : depth_first(&DT)) {
    for (Instruction &I : *Node->getBlock()) {
      if (!CandidateOp::canDistributeInst(I))
        continue;

      unsigned N = I.isCommutative() ? 2 : 1;
      for (unsigned Op = 0; Op != N; ++Op) {
        auto [It, _] =
            Grouped.try_emplace({I.getOpcode(), I.getOperand(1 - Op)}, DT);
        RedistributeCandidates &Candidates = It->second;
        Use &Over = I.getOperandUse(Op);
        Candidates.add(Over);
      }
    }
  }

  SmallVector<RedistributeCandidates> AllCandidates;
  for (auto &[_, Candidates] : Grouped)
    if (!Candidates.empty())
      AllCandidates.emplace_back(std::move(Candidates));

  return AllCandidates;
}

static SmallVector<RedistributeCandidates>
collectCandidates(Function &F, DominatorTree &DT) {

  auto AllCandidates = collectInitialRedistributeCandidates(F, DT);
  for (RedistributeCandidates &C : AllCandidates)
    C.removeTriviallyUnprofitableCandidates();

  AllCandidates.erase(
      remove_if(AllCandidates, std::mem_fn(&RedistributeCandidates::empty)),
      AllCandidates.end());

  auto BySize = [](const RedistributeCandidates &L,
                   const RedistributeCandidates &R) {
    return L.size() < R.size();
  };
  stable_sort(AllCandidates, BySize);

  return AllCandidates;
}

static bool runImpl(Function &F, DominatorTree &DT,
                    const TargetTransformInfo &TTI) {
  bool Changed = false;
  bool ChangedInThisIteration;
  do {
    ChangedInThisIteration = false;
    SmallVector<RedistributeCandidates> Candidates = collectCandidates(F, DT);
    for (RedistributeCandidates &C : Candidates) {
      if (C.tryToRedistribute(TTI)) {
        Changed = ChangedInThisIteration = true;
        break;
      }
    }
  } while (ChangedInThisIteration);
  return Changed;
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
    const TargetTransformInfo &TTI =
        getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
    return runImpl(F, DT, TTI);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
    AU.setPreservesCFG();
  }
};

char RedistributeLegacyPass::ID = 0;
} // namespace

PreservedAnalyses RedistributePass::run(Function &F,
                                        FunctionAnalysisManager &AM) {
  DominatorTree &DT = AM.getResult<DominatorTreeAnalysis>(F);
  const TargetTransformInfo &TTI = AM.getResult<TargetIRAnalysis>(F);
  if (!runImpl(F, DT, TTI))
    return PreservedAnalyses::all();

  PreservedAnalyses PA;
  PA.preserveSet<CFGAnalyses>();
  return PA;
}

INITIALIZE_PASS_BEGIN(RedistributeLegacyPass, "redistribute", "Redistribute",
                      false, false)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(RedistributeLegacyPass, "redistribute", "Redistribute",
                    false, false)

FunctionPass *llvm::createRedistributePass() {
  return new RedistributeLegacyPass();
}
