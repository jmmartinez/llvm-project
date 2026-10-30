//===-- SQTTDump.cpp - SQTT event table dumper ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file implements the SQTT event table dumper for llvm-objdump.
///
//===----------------------------------------------------------------------===//

#include "SQTTDump.h"
#include "llvm-objdump.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/TargetParser/Triple.h"

using namespace llvm;

void llvm::dumpSQTT(const object::ObjectFile &O) {
  if (!O.isELF()) {
    objdump::reportWarning("SQTT is currently only supported for ELF targets",
                           O.getFileName());
    return;
  }

  if (O.getArch() != Triple::amdgpu) {
    objdump::reportWarning("SQTT is only supported for AMDGPU targets",
                           O.getFileName());
    return;
  }
}
