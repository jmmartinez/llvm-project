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
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLForwardCompat.h"
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/DebugInfo/DWARF/DWARFContext.h"
#include "llvm/IR/AMDGPUSQTTEventType.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/RelocationResolver.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/AMDGPU/SQTTEvent.h"
#include "llvm/TargetParser/Triple.h"

#include <optional>

using namespace llvm;

template <typename T> static T readValue(StringRef Data, uint64_t Offset) {
  return support::endian::read<T, llvm::endianness::little>(Data.data() +
                                                            Offset);
}

// Relocatable objects store pointer fields as R_AMDGPU_ABS64 addends. Those
// relocations live in a separate section. A linked image already has the
// values in the section bytes.
static DenseMap<uint64_t, uint64_t>
relocatedValues(const object::SectionRef &Target) {
  const object::ObjectFile *O = Target.getObject();
  DenseMap<uint64_t, uint64_t> Values;
  auto [Supports, Resolver] = object::getRelocationResolver(*O);
  if (!Supports)
    return Values;

  for (const object::SectionRef &Sec : O->sections()) {
    Expected<object::section_iterator> RelocatedOrErr =
        Sec.getRelocatedSection();
    if (!RelocatedOrErr) {
      consumeError(RelocatedOrErr.takeError());
      continue;
    }
    if (*RelocatedOrErr == O->section_end() || **RelocatedOrErr != Target)
      continue;

    for (const object::RelocationRef &Reloc : Sec.relocations()) {
      if (!Supports(Reloc.getType())) {
        objdump::reportWarning("unsupported relocation type " +
                                   Twine(Reloc.getType()),
                               O->getFileName());
        continue;
      }
      object::symbol_iterator Sym = Reloc.getSymbol();
      if (Sym == O->symbol_end())
        continue;
      uint64_t SymValue =
          objdump::unwrapOrError(Sym->getValue(), O->getFileName());
      Values[Reloc.getOffset()] =
          object::resolveRelocation(Resolver, Reloc, SymValue, /*LocData=*/0);
    }
  }
  return Values;
}

static uint64_t readPointer(StringRef Data, uint64_t Offset,
                            const DenseMap<uint64_t, uint64_t> &Relocs) {
  auto It = Relocs.find(Offset);
  if (It != Relocs.end())
    return It->second;
  return readValue<uint64_t>(Data, Offset);
}

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

static bool hasStringPayload(uint16_t Type) {
  using sqtt::EventType;
  switch (Type) {
  case to_underlying(EventType::FunctionEntry):
  case to_underlying(EventType::FunctionExit):
  case to_underlying(EventType::UserEntry):
  case to_underlying(EventType::UserExit):
    return true;
  default:
    return false;
  }
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

  std::optional<object::SectionRef> Events, DataStr;
  // The data section is named after the format version of its records.
  SmallVector<object::SectionRef, 1> Data;
  for (const object::SectionRef &Section : O.sections()) {
    StringRef SectionName =
        objdump::unwrapOrError(Section.getName(), O.getFileName());
    if (SectionName == sqtt::EventsTableSection)
      Events = Section;
    else if (SectionName == sqtt::DataStringsSection)
      DataStr = Section;
    else if (SectionName.starts_with(sqtt::DataTableSectionPrefix))
      Data.push_back(Section);
  }

  auto CheckFound = [&](size_t Count, StringRef Name) {
    if (Count == 1)
      return true;
    // Linking compilation units built with different format versions leaves
    // one data section per version.
    objdump::reportWarning(Count ? "multiple SQTT sections " + Name
                                 : "SQTT section " + Name + " not found",
                           O.getFileName());
    return false;
  };
  if (!CheckFound(Data.size(), sqtt::DataTableSectionPrefix))
    return;
  if (!CheckFound(DataStr.has_value(), sqtt::DataStringsSection))
    return;
  if (!CheckFound(Events.has_value(), sqtt::EventsTableSection))
    return;

  StringRef DataSectionName =
      objdump::unwrapOrError(Data.front().getName(), O.getFileName());
  StringRef VersionStr = DataSectionName;
  VersionStr.consume_front(sqtt::DataTableSectionPrefix);
  uint32_t Version;
  if (VersionStr.getAsInteger(10, Version) || Version != sqtt::FormatVersion) {
    objdump::reportWarning("SQTT section " + DataSectionName +
                               " has an unsupported format version",
                           O.getFileName());
    return;
  }

  StringRef EventsArray =
      objdump::unwrapOrError(Events->getContents(), O.getFileName());
  StringRef DataArray =
      objdump::unwrapOrError(Data.front().getContents(), O.getFileName());
  uint64_t DataBase = Data.front().getAddress();
  StringRef DataStringsArray =
      objdump::unwrapOrError(DataStr->getContents(), O.getFileName());
  uint64_t StrBase = DataStr->getAddress();
  DenseMap<uint64_t, uint64_t> EventRelocs = relocatedValues(*Events);
  DenseMap<uint64_t, uint64_t> DataRelocs = relocatedValues(Data.front());
  std::unique_ptr<DWARFContext> DICtx = DWARFContext::create(O);

  // Each data record is {u64 pc, u16 count, [u16 type, u64 payload]+}.
  json::OStream J(outs(), 2);
  J.array([&] {
    for (uint64_t Offset = 0; Offset < EventsArray.size(); Offset += 8) {
      uint64_t EventDataOffset =
          readPointer(EventsArray, Offset, EventRelocs) - DataBase;
      uint64_t PC = readPointer(DataArray, EventDataOffset, DataRelocs);
      uint16_t EventCount = readValue<uint16_t>(DataArray, EventDataOffset + 8);
      J.array([&] {
        uint64_t EventOffset = EventDataOffset + 10;
        for (uint16_t I = 0; I < EventCount; ++I) {
          uint16_t EventType = readValue<uint16_t>(DataArray, EventOffset);
          uint64_t Payload =
              readPointer(DataArray, EventOffset + 2, DataRelocs);
          J.object([&] {
            J.attribute("type", eventTypeName(EventType));
            if (!hasStringPayload(EventType)) {
              J.attribute("payload", Payload);
            } else if (Payload >= StrBase &&
                       Payload - StrBase < DataStringsArray.size()) {
              StringRef Str = DataStringsArray.substr(Payload - StrBase);
              Str = Str.substr(0, Str.find('\0'));
              J.attribute("payload", Str);
            } else {
              objdump::reportWarning("SQTT string payload " + Twine(Payload) +
                                         " is outside __sqtt_data_str",
                                     O.getFileName());
              J.attribute("payload", Payload);
            }
            std::optional<DILineInfo> Line = lineForPC(*DICtx, O, PC);
            if (Line) {
              J.attribute("file", Line->FileName);
              J.attribute("line", Line->Line);
              J.attribute("column", Line->Column);
            }
          });
          EventOffset += 10;
        }
      });
    }
  });
  outs() << '\n';
}
