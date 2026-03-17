#include "Common.h"
#include <algorithm>

namespace opt {

Pass *makeHighGVN(ModuleOp *module);

#define non_hoistable_list(X) \
  X(DoWhileOp) X(IfOp) X(ConditionOp) X(YieldOp) X(BreakOp) X(ContinueOp) X(ReturnOp) \
  X(StoreOp) X(ArrayStoreOp) X(CallOp) X(ExternCallOp)

#define non_hoistable_impl(Ty) \
  isa<Ty>(op) ||

bool nonHoistable(Op *op) {
  return non_hoistable_list(non_hoistable_impl) false;
}

declare_local_pass(LICM,
  void lift(DoWhileOp *loop);
  std::set<DoWhileOp*> visited;
) {
  for_all(DoWhileOp, func)
    lift(op);

  // makeHighGVN(module)->run();
}

std::set<Value*> getVariantsIn(DoWhileOp *loop) {
  std::set<Value*> variants(loop->getResults().begin(), loop->getResults().end());
  std::set<Value*> storedGlobals;

  // To do this, we must ensure GVN is performed and PropagateArray is done, to
  // distinguish different arrays.
  bool called = false;
  Pass::walk(loop, [&](Op *op) {
    if (isa<ArrayStoreOp>(op) || isa<StoreOp>(op))
      storedGlobals.insert(op->val());
    called = true;
  });

  Pass::walk(loop, [&](Op *op) {
    bool variant = false;
    for (auto x : op->getOperands()) {
      if (variants.count(x)) {
        variant = true;
        break;
      }
    }

    if ((isa<ArrayLoadOp>(op) || isa<LoadOp>(op)) && (storedGlobals.count(op->val()) || called))
      variant = true;
    if (nonHoistable(op))
      variant = true;

    if (variant) {
      for (auto r : op->getResults())
        variants.insert(r);
    }
  });

  return variants;
}

void LICM::lift(DoWhileOp *loop) {
  if (visited.count(loop))
    return;
  visited.insert(loop);

  for_all(DoWhileOp, loop)
    lift(op);
  
  // First find loop variants.
  auto variants = getVariantsIn(loop);
  walk(loop, [&](Op *op) {
    if (std::any_of(op->getResults().begin(), op->getResults().end(), [&](Value *v) { return variants.count(v); }))
      return;
    if (nonHoistable(op))
      return;

    op->moveBefore(loop);
  });
}

}
