#include "Common.h"

namespace opt {

declare_local_pass(TCO) {
  std::vector<ReturnOp*> toChange;
  ReturnOp *redundant = nullptr;
  for_all(ReturnOp, func) {
    // It is possible that there's an extra return at the end of the function.
    if (!op->getNumOperands()) {
      if (op != func->getRegion()->getLastOp())
        return;
      redundant = op;
      continue;
    }

    if (auto ret = op->val()->def; isa<CallOp>(ret)) {
      if (cast<FuncOp>(ret->val()->def)->name != func->name)
        return;
      // The call must only be used by the return.
      if (ret->ret()->getUses().size() != 1)
        return;

      toChange.push_back(op);
    }
  }
  if (toChange.empty())
    return;
  if (redundant)
    redundant->erase();

  // Now we know `func` is tail-recursive.
  // We can put the entire function inside a DoWhileOp.
  Builder builder;

  // Create a temporary region for the while.
  auto region = func->appendRegion();
  builder.setToEnd(region);
  
  auto loop = builder.create<DoWhileOp>();
  auto loopRegion = loop->appendRegion();
  builder.setBefore(loop);
  auto one = builder.createInt(1);

  // Even though it is unreachable, the IR needs this return to work.
  builder.setAfter(loop);
  builder.create<ReturnOp>();

  builder.setToEnd(loopRegion);
  auto marker = builder.create<CondMarkerOp>(); // For EnsureTerminator to work.
  builder.create<ConditionOp>()->with(one->ret());

  auto oldRegion = func->getRegion(0);
  oldRegion->getFirstBlock()->inlineBefore(marker);
  func->removeRegion(oldRegion);

  // Identify the allocas for arguments, and hoist them and stores to them out of the loop.
  std::vector<AllocaOp *> addr;
  addr.resize(func->getNumResults() - 1);
  for_all(AllocaOp) {
    StoreOp *store = nullptr;
    bool bad = false;
    for (auto use : op->ret()->getUses()) {
      if (!isa<StoreOp>(use))
        continue;

      if (store) {
        bad = true;
        break;
      }
      store = cast<StoreOp>(use);
    }
    if (bad || !store)
      continue;

    auto val = store->val(1);
    if (auto i = func->getResultIndex(val); i != func->getNumResults()) {
      addr[i - 1] = op;
      op->moveBefore(loop);
      store->moveBefore(loop);
    }
  }
  assert(std::all_of(addr.begin(), addr.end(), [](Op *op) { return bool(op); }));

  for (auto op : toChange) {
    builder.setBefore(op);
    auto call = op->val()->def;

    for (unsigned i = 1; i < call->getNumOperands(); i++) {
      auto alloca = addr[i - 1];
      builder.create<StoreOp>()->with(alloca->ret(), call->val(i));
    }
    builder.replace<ContinueOp>(op);
    call->erase();
  }
}

}
