#include "Common.h"

namespace opt {

declare_local_pass(LowerPostSchedule) {
  Builder builder;

  for_all(BlOp, func) {
    builder.setBefore(op);
    int args = 0, fargs = 0;
    for (auto val : op->getOperands()) {
      auto wr = builder.create<WriteRegOp>()->with(val);
      if (regbank(val->type) == INT) {
        assert(args < 8); // TODO
        wr->reg = argRegs[args++];
      } else {
        assert(fargs < 8); // TODO
        wr->reg = fargRegs[fargs++];
      }
    }
    op->clearOperands();
    
    builder.setAfter(op);
    if (op->getNumResults() > 0) {
      auto ret = op->ret();
      if (ret->type != unit) {
        auto rd = builder.create<ReadRegOp>(ret->type);
        rd->reg = x0;
        ret->replaceAllUsesWith(rd->ret());
      }
      op->clearResults();
    }

    // Give functions more results, to mark conflicts.
    for (auto reg : callerSaved) {
      auto value = op->pushResult(i64);
      assignment[value] = reg;
    }
  }

  for_all(ReturnOp, func) {
    builder.setBefore(op);
    if (op->getNumOperands() > 0) {
      auto wr = builder.create<WriteRegOp>()->with(op->val());
      wr->reg = x0;
    }

    builder.rename<RetOp>(op);
  }
};

}
