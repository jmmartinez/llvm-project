#ifndef LLVM_FRONTEND_SQTT_EVENT_H
#define LLVM_FRONTEND_SQTT_EVENT_H

namespace llvm {
class Function;
class LLVMContext;
class Metadata;
class Value;

namespace sqtt {

enum class EventType {
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

  Metadata *toMetadata(LLVMContext &Ctx) const;
  static Event fromMetadata(Metadata *MD);
};

} // namespace sqtt
} // namespace llvm

#endif
