#include "llvm/Frontend/SQTT/Event.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"

using namespace llvm;
using namespace llvm::sqtt;

Metadata *Event::toMetadata(LLVMContext &Ctx) const {
  auto *IntTy = Type::getInt32Ty(Ctx);
  Metadata *TypeMD = ConstantAsMetadata::get(
      ConstantInt::get(IntTy, static_cast<uint32_t>(Type)));
  Metadata *PayloadMD = ValueAsMetadata::get(Payload);
  return MDNode::get(Ctx, {TypeMD, PayloadMD});
}

Event Event::fromMetadata(Metadata *MD) {
  auto *Node = cast<MDNode>(MD);

  Metadata *TypeMD = Node->getOperand(0);
  ConstantAsMetadata *TypeCA = cast<ConstantAsMetadata>(TypeMD);
  ConstantInt *TypeCI = cast<ConstantInt>(TypeCA->getValue());
  EventType Type = static_cast<EventType>(TypeCI->getZExtValue());

  Metadata *PayloadMD = Node->getOperand(1);
  Value *Payload = cast<ValueAsMetadata>(PayloadMD)->getValue();

  return {Type, Payload};
}
