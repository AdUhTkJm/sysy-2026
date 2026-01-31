#include "Ops.h"
#include "../utils/DataStructure.h"

namespace ir {

Value *FuncOp::getHandle() const {
  return ret(0);
}

std::vector<Value*> FuncOp::getArgs() const {
  return std::vector(results.begin() + 1, results.end());
}

Value *FuncOp::getArg(unsigned i) const {
  return ret(i + 1);
}

void PhiOp::addIncoming(Value *v, Block *bb) {
  pushOperand(v);
  targets.push_back(bb);
}

Value *PhiOp::incomingFrom(const Block *bb) const {
  for (auto [i, t] : data::enumerate(targets)) {
    if (t == bb)
      return val(i);
  }
  return nullptr;
}

#define isa(Ty) isa<Ty>(op) ||
bool isPure(Op *op) {
  return !(impure_op_list(isa) true);
}
#undef isa

}
