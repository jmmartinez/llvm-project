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
#include "llvm/Config/Targets.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/AMDGPU/SQTTEvent.h"

#include <optional>

using namespace llvm;

#if LLVM_HAS_AMDGPU_TARGET
// The PC field is the address of s_ttracedata. In a relocatable object that
// address is an offset into .text; in a linked image it is a virtual address.
static std::optional<object::SectionedAddress>
codeAddressForPC(const object::ObjectFile &O, uint64_t PC) {
  for (const object::SectionRef &Sec : O.sections()) {
    if (!Sec.isText())
      continue;
    uint64_t Start = Sec.getAddress();
    uint64_t Size = Sec.getSize();
    if (PC >= Start && PC < Start + Size)
      return object::SectionedAddress{PC, Sec.getIndex()};
  }
  return std::nullopt;
}

static std::optional<DILineInfo>
lineForPC(DWARFContext &DICtx, const object::ObjectFile &O, uint64_t PC) {
  std::optional<object::SectionedAddress> Addr = codeAddressForPC(O, PC);
  if (!Addr)
    return std::nullopt;

  DILineInfoSpecifier Spec(DILineInfoSpecifier::FileLineInfoKind::BaseNameOnly);
  std::optional<DILineInfo> Info = DICtx.getLineInfoForAddress(*Addr, Spec);
  if (!Info || Info->FileName.empty() ||
      Info->FileName == DILineInfo::BadString)
    return std::nullopt;
  return Info;
}

static StringRef eventTypeName(sqtt::EventType Type) {
  switch (Type) {
  case sqtt::EventType::FunctionEntry:
    return "FunctionEntry";
  case sqtt::EventType::FunctionExit:
    return "FunctionExit";
  case sqtt::EventType::UserEntry:
    return "UserEntry";
  case sqtt::EventType::UserExit:
    return "UserExit";
  case sqtt::EventType::SetEventTableBase:
    return "SetEventTableBase";
  case sqtt::EventType::End:
    break;
  }
  llvm_unreachable("invalid SQTT event type");
}

void llvm::dumpSQTT(const object::ObjectFile &O) {
  Expected<std::vector<sqtt::BinarySQTTRecord>> Records =
      sqtt::parseBinarySQTTRecords(O);
  if (!Records) {
    objdump::reportWarning(toString(Records.takeError()), O.getFileName());
    return;
  }

  std::unique_ptr<DWARFContext> DICtx = DWARFContext::create(O);

  json::OStream J(outs(), 2);
  J.array([&] {
    for (const sqtt::BinarySQTTRecord &Record : *Records) {
      J.object([&] {
        uint64_t PC = Record.getTTraceDataAddress();
        J.attribute("pc", PC);
        if (std::optional<DILineInfo> Line = lineForPC(*DICtx, O, PC)) {
          J.attribute("file", Line->FileName);
          J.attribute("line", Line->Line);
          J.attribute("column", Line->Column);
        }
        J.attributeArray("events", [&] {
          for (const sqtt::Event &Event : Record.events()) {
            J.object([&] {
              J.attribute("type", eventTypeName(Event.getType()));
              if (sqtt::Event::hasStringPayload(Event.getType()))
                J.attribute("payload", Event.getPayload());
              else
                J.attribute("payload", Event.getIndex());
            });
          }
        });
      });
    }
  });
  outs() << '\n';
}
#else
void llvm::dumpSQTT(const object::ObjectFile &O) {
  objdump::reportWarning("SQTT requires the AMDGPU target", O.getFileName());
}
#endif
