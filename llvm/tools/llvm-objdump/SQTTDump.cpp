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

static bool hasFile(const DILineInfo &Info) {
  return !Info.FileName.empty() && Info.FileName != DILineInfo::BadString;
}

// Returns the source locations of PC: element 0 is the location of PC itself,
// and element I is the location where the code of element I-1 was inlined at.
// The list stops at the first frame without a known file.
static SmallVector<DILineInfo, 4>
locationsForPC(DWARFContext &DICtx, const object::ObjectFile &O, uint64_t PC) {
  SmallVector<DILineInfo, 4> Locations;
  std::optional<object::SectionedAddress> Addr = codeAddressForPC(O, PC);
  if (!Addr)
    return Locations;

  DILineInfoSpecifier Spec(DILineInfoSpecifier::FileLineInfoKind::BaseNameOnly);
  DIInliningInfo Info = DICtx.getInliningInfoForAddress(*Addr, Spec);
  for (uint32_t I = 0, N = Info.getNumberOfFrames(); I < N; ++I) {
    const DILineInfo &Frame = Info.getFrame(I);
    if (!hasFile(Frame))
      break;
    Locations.push_back(Frame);
  }
  return Locations;
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
        SmallVector<DILineInfo, 4> Locations = locationsForPC(*DICtx, O, PC);
        if (!Locations.empty()) {
          J.attributeArray("locations", [&] {
            for (const DILineInfo &Loc : Locations) {
              J.object([&] {
                J.attribute("file", Loc.FileName);
                J.attribute("line", Loc.Line);
                J.attribute("column", Loc.Column);
              });
            }
          });
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
