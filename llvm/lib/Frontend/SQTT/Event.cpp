#include "llvm/Frontend/SQTT/Event.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"

using namespace llvm;
using namespace llvm::sqtt;

Event Event::functionEntry(const Function &F) {
  return Event(EventType::FunctionEntry, F.getName());
}

Event Event::functionExit(const Function &F) {
  return Event(EventType::FunctionExit, F.getName());
}

Metadata *Event::toMetadata(LLVMContext &Ctx) const {
  auto *IntTy = Type::getInt32Ty(Ctx);
  Metadata *TypeMD = ConstantAsMetadata::get(
      ConstantInt::get(IntTy, static_cast<uint32_t>(Type)));
  Metadata *PayloadMD = MDString::get(Ctx, Payload);
  return MDNode::get(Ctx, {TypeMD, PayloadMD});
}

Event Event::fromMetadata(Metadata *MD) {
  auto *Node = cast<MDNode>(MD);

  Metadata *TypeMD = Node->getOperand(0);
  ConstantAsMetadata *TypeCA = cast<ConstantAsMetadata>(TypeMD);
  ConstantInt *TypeCI = cast<ConstantInt>(TypeCA->getValue());
  EventType Type = static_cast<EventType>(TypeCI->getZExtValue());

  StringRef Payload = cast<MDString>(Node->getOperand(1))->getString();
  return {Type, Payload};
}

MergedEvent MergedEvent::fromMetadata(Metadata *MD) {
  auto *Node = cast<MDNode>(MD);
  Metadata *Op0 = Node->getOperand(0);
  if (isa<ConstantAsMetadata>(Op0)) {
    return {{Event::fromMetadata(MD)}};
  }

  SmallVector<Event, 4> Events;
  for (unsigned I = 0; I != Node->getNumOperands(); ++I) {
    Events.push_back(Event::fromMetadata(Node->getOperand(I)));
  }
  return {Events};
}

Metadata *MergedEvent::toMetadata(LLVMContext &Ctx) const {
  SmallVector<Metadata *, 4> Ops;
  for (const Event &Ev : Events)
    Ops.push_back(Ev.toMetadata(Ctx));

  if (Ops.size() == 1)
    return Ops.front();

  return MDNode::get(Ctx, Ops);
}
