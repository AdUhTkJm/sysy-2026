#include "Common.h"

namespace opt {

declare_pass(DSE) {
  std::set<Op *> read;
  bool unbased = false;
  for_all(LoadOp) {
    if (auto base = baseOf(op->val()); base && !isa<FuncOp>(base))
      read.insert(base);
    else unbased = true;
  }
  for_all(ArrayLoadOp) {
    if (auto base = baseOf(op->val()); base && !isa<FuncOp>(base))
      read.insert(base);
    else unbased = true;
  }
  for_all(ExternCallOp) {
    for (auto operand : op->getOperands()) {
      if (auto base = baseOf(operand))
        read.insert(base);
    }
  }
  if (unbased)
    return;

  for_all(StoreOp) {
    if (auto base = baseOf(op->val()); !read.count(base))
      op->erase();
  }
  for_all(ArrayStoreOp) {
    if (auto base = baseOf(op->val()); !read.count(base))
      op->erase();
  }
}

}
