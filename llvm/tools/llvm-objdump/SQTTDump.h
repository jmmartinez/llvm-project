//===-- SQTTDump.h - SQTT event table dumper --------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_OBJDUMP_SQTTDUMP_H
#define LLVM_TOOLS_LLVM_OBJDUMP_SQTTDUMP_H

namespace llvm {

namespace object {
class ObjectFile;
} // namespace object

void dumpSQTT(const object::ObjectFile &O);

} // namespace llvm

#endif
