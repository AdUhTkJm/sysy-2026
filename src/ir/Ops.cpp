#include "Ops.h"

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

}
