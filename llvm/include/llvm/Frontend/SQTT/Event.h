#ifndef LLVM_FRONTEND_SQTT_EVENT_H
#define LLVM_FRONTEND_SQTT_EVENT_H

#include "llvm/ADT/StringRef.h"

#include <cstdint>

namespace llvm {
class Function;
class LLVMContext;
class Metadata;

namespace sqtt {

const char EventsTableMetadata[] = "llvm.sqtt.events";
const char EventsTableSection[] = "__sqtt_events";
const char StringsTableSection[] = "__sqtt_strings";

enum class EventType : uint16_t {
  FunctionEntry,
  FunctionExit,
};

class Event {
  EventType Type;
  StringRef Payload;

  Event(EventType Type, StringRef Payload) : Type(Type), Payload(Payload) {}

public:
  static Event functionEntry(const Function &F);
  static Event functionExit(const Function &F);

  EventType getType() const { return Type; }
  StringRef getPayload() const { return Payload; }

  Metadata *toMetadata(LLVMContext &Ctx) const;
  static Event fromMetadata(Metadata *MD);
};

} // namespace sqtt
} // namespace llvm

#endif
