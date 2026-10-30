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
#include "llvm/ADT/STLForwardCompat.h"
#include "llvm/IR/AMDGPUSQTTEventType.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Triple.h"

#include <map>
#include <optional>

using namespace llvm;

template <typename T> static T readValue(StringRef Data, uint64_t Offset) {
  return support::endian::read<T, llvm::endianness::little>(Data.data() +
                                                            Offset);
}

static StringRef eventTypeName(uint16_t Type) {
  using sqtt::EventType;
  switch (Type) {
  case to_underlying(EventType::FunctionEntry):
    return "FunctionEntry";
  case to_underlying(EventType::FunctionExit):
    return "FunctionExit";
  case to_underlying(EventType::UserEntry):
    return "UserEntry";
  case to_underlying(EventType::UserExit):
    return "UserExit";
  case to_underlying(EventType::SetEventTableBase):
    return "SetEventTableBase";
  default:
    break;
  }
  return "<unknown>";
}

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

  std::map<StringRef, std::optional<object::SectionRef>> Sections{
      {"__sqtt_events", std::nullopt},
      {"__sqtt_data", std::nullopt},
      {"__sqtt_data_str", std::nullopt}};

  for (const object::SectionRef &Section : O.sections()) {
    StringRef SectionName =
        objdump::unwrapOrError(Section.getName(), O.getFileName());
    auto Sec = Sections.find(SectionName);
    if (Sec != Sections.end())
      Sec->second = Section;
  }

  bool FoundAllSections = true;
  for (const auto &[Name, Section] : Sections) {
    if (Section)
      continue;

    FoundAllSections = false;
    objdump::reportWarning("SQTT section " + Name + " not found",
                           O.getFileName());
  }

  if (!FoundAllSections)
    return;

  object::SectionRef &SqttEvents = *Sections["__sqtt_events"];
  object::SectionRef &SqttData = *Sections["__sqtt_data"];

  StringRef EventsArray =
      objdump::unwrapOrError(SqttEvents.getContents(), O.getFileName());
  StringRef DataArray =
      objdump::unwrapOrError(SqttData.getContents(), O.getFileName());
  uint64_t DataBase = SqttData.getAddress();

  // Each __sqtt_data record is {u64 pc, u16 count, [u16 type, u64 payload]+}.
  json::OStream J(outs(), 2);
  J.array([&] {
    for (uint64_t Offset = 0; Offset < EventsArray.size(); Offset += 8) {
      uint64_t EventDataOffset =
          readValue<uint64_t>(EventsArray, Offset) - DataBase;
      uint64_t PC = readValue<uint64_t>(DataArray, EventDataOffset);
      uint16_t EventCount = readValue<uint16_t>(DataArray, EventDataOffset + 8);
      J.array([&] {
        uint64_t EventOffset = EventDataOffset + 10;
        for (uint16_t I = 0; I < EventCount; ++I) {
          uint16_t EventType = readValue<uint16_t>(DataArray, EventOffset);
          uint64_t Payload = readValue<uint64_t>(DataArray, EventOffset + 2);
          J.object([&] {
            J.attribute("type", eventTypeName(EventType));
            J.attribute("payload", Payload);
            J.attribute("address", PC);
          });
          EventOffset += 10;
        }
      });
    }
  });
  outs() << '\n';
}
