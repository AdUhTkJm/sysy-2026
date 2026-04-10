#include "Common.h"

namespace opt {

declare_local_pass(LowerPostSchedule) {
  Builder builder;

  for_all(BlOp, func) {
    std::vector<const Type*> types;
    types.reserve(op->getNumOperands());
    for (auto val : op->getOperands())
      types.push_back(val->type);

    std::vector<ArgLoc> locs;
    int stackBytes = argLayout(types, locs);
    assert(locs.size() == types.size());

    builder.setBefore(op);
    ReadRegOp *sp = nullptr;
    if (stackBytes > 0) {
      auto byte = stackBytes;
      if (auto off = func->get<StackOffsetAttr>())
        byte = std::max(byte, off->size);
      func->set<StackOffsetAttr>(byte);
      sp = createAssignedRd(builder, Reg::sp);
    }

    for (unsigned i = 0; i < op->getNumOperands(); i++) {
      auto val = op->val(i);
      const auto &loc = locs[i];
      if (!loc.inReg) {
        auto st = builder.create<StrOp>()->with(sp->ret(), val);
        st->value = loc.stackOffset;
        st->set<IncomingStackArgAttr>();
      } else {
        auto wr = builder.create<WriteRegOp>(unit)->with(val);
        wr->reg = loc.reg;
        assignment[wr->ret()] = (Reg) wr->reg;
      }
    }
    op->clearOperands();
    
    builder.setAfter(op);
    if (op->getNumResults() > 0) {
      auto ret = op->ret();
      if (ret->type != unit) {
        auto rd = builder.create<ReadRegOp>(ret->type);
        rd->reg = regbank(ret->type) == INT ? x0 : v0;
        ret->replaceAllUsesWith(rd->ret());
      }
      op->clearResults();
    }

    // Give functions more results, to mark conflicts.
    for (auto reg : callerSaved) {
      auto value = op->pushResult(regbank(reg) == INT ? i32 : f32);
      assignment[value] = reg;
    }
  }

  for_all(ReturnOp, func) {
    builder.setBefore(op);
    if (op->getNumOperands() > 0) {
      auto wr = builder.create<WriteRegOp>()->with(op->val());
      wr->reg = regbank(op->val()->type) == INT ? x0 : v0;
    }

    builder.rename<RetOp>(op);
  }
};

}
