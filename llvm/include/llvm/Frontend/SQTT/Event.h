#ifndef LLVM_FRONTEND_SQTT_EVENT_H
#define LLVM_FRONTEND_SQTT_EVENT_H

#include <cstdint>

#include "llvm/Support/Casting.h"

namespace llvm {
class Function;
class LLVMContext;
class Metadata;
class Value;

namespace sqtt {

const char EventsTableMetadata[] = "llvm.sqtt.events";
const char EventsTableSection[] = "__sqtt_events";

enum class EventType : uint16_t {
  FunctionEntry,
  FunctionExit,
};

class Event {
  EventType Type;
  Value *Payload;

  Event(EventType Type, Value *Payload) : Type(Type), Payload(Payload) {}

public:
  static Event functionEntry(Function &F) {
    return Event(EventType::FunctionEntry, &F);
  }

  static Event functionExit(Function &F) {
    return Event(EventType::FunctionExit, &F);
  }

  EventType getType() const { return Type; }
  Value *getPayload() const { return Payload; }
  GlobalValue *getPayloadAsGlobalValue() const {
    return cast<GlobalValue>(Payload);
  }

  Metadata *toMetadata(LLVMContext &Ctx) const;
  static Event fromMetadata(Metadata *MD);
};

} // namespace sqtt
} // namespace llvm

#endif
