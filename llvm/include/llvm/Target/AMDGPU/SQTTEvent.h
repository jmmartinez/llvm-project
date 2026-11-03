//===- SQTTEvent.h - AMDGPU SQTT event metadata -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// SQTT event metadata shared by the AMDGPU backend.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGET_AMDGPU_SQTTEVENT_H
#define LLVM_TARGET_AMDGPU_SQTTEVENT_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/AMDGPUSQTTEventType.h"
#include <cstdint>

namespace llvm {
class Function;
class LLVMContext;
class Metadata;

namespace sqtt {

const char EventsTableMetadata[] = "llvm.sqtt.events";
const char EventsTableSection[] = "__sqtt_events";
const char DataTableSection[] = "__sqtt_data";
const char DataStringsSection[] = "__sqtt_data_str";
const char EventsTableCUStart[] = "sqtt_events_cu_base";

constexpr uint32_t SectionBase = 0xffffffff;

class Event {
  EventType Type;
  union {
    StringRef StringPayload;
    uint32_t IntPayload;
  };

  Event(EventType Type, StringRef Payload)
      : Type(Type), StringPayload(Payload) {}
  Event(EventType Type, uint32_t Payload) : Type(Type), IntPayload(Payload) {}

public:
  static Event functionEntry(const Function &F);
  static Event functionExit(const Function &F);
  static Event userEntry(StringRef Payload);
  static Event userExit(StringRef Payload);
  static Event setEventTableBase(uint32_t Index);

  EventType getType() const { return Type; }
  bool isSetEventTableBase() const {
    return Type == EventType::SetEventTableBase;
  }
  StringRef getPayload() const { return StringPayload; }
  uint32_t getIndex() const { return IntPayload; }

  Metadata *toMetadata(LLVMContext &Ctx) const;
  static Event fromMetadata(Metadata *MD);
};

class MergedEvent {
  SmallVector<Event, 4> Events;

public:
  MergedEvent(ArrayRef<Event> Events) : Events(Events) {}
  ArrayRef<Event> events() const { return Events; }
  size_t size() const { return Events.size(); }

  Metadata *toMetadata(LLVMContext &Ctx) const;
  static MergedEvent fromMetadata(Metadata *MD);
};

} // namespace sqtt
} // namespace llvm

#endif
