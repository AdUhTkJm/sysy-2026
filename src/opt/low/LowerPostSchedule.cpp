#include "Common.h"

namespace opt {

declare_local_pass(LowerPostSchedule) {
  Builder builder;

  for_all(ReturnOp, func) {
    builder.setBefore(op);
    if (op->getNumOperands() > 0) {
      auto rd = builder.create<WriteRegOp>()->with(op->val());
      rd->reg = x0;
    }

    builder.rename<RetOp>(op);
  }
};

}
