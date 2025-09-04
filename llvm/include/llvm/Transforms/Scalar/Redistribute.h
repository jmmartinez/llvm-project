//===- Redistribute.h - Redistribute k*(a+...b) expressions - C++ -*-=========//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass distributes distributable operations over additions that share
// common terms, in order to reduce the number of operations in the code.
//
// It converts:
//
//   f(a*k)
//   f((a+b)*k)
//   f(((a+b)+b)*k)
//
// into:
//
//   ak = a*k
//   bk = b*k
//   f(ak)
//   f(ak+bk)
//   f((ak+bk)+bk)
//
// Saving 1 multiplication.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SCALAR_REDISTRIBUTE_H
#define LLVM_TRANSFORMS_SCALAR_REDISTRIBUTE_H

#include "llvm/IR/PassManager.h"

namespace llvm {
class DominatorTree;

class RedistributePass : public PassInfoMixin<RedistributePass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};
} // namespace llvm
#endif
