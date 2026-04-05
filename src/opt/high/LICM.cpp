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
