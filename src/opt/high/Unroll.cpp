#include "Common.h"

namespace opt {

declare_local_pass(Unroll,
  bool attemptUnroll(DoWhileOp *loop);
) {
  fixed(for_all(DoWhileOp, func)
    if (attemptUnroll(op))
      break;
  )
}

bool Unroll::attemptUnroll(DoWhileOp *loop) {
  // Try to unroll constant-stepped loops only.
  Value *limit = nullptr;
  auto ind = indvar(loop, &limit);

  if (!ind || !limit)
    return false;

  auto stepValue = increment(ind);
  auto step = stepValue ? dyn_cast<IntOp>(stepValue->def) : nullptr;
  if (!step || step->value <= 0)
    return false;

  auto last = condition_of(loop);
  auto cond = dyn_cast<LtOp>(last->val(0)->def);
  auto add = cond ? dyn_cast<AddIOp>(cond->val(0)->def) : nullptr;
  if (!add)
    return false;

  auto condStep = add->val(0) == ind ? add->val(1)
                : add->val(1) == ind ? add->val(0)
                : nullptr;
  auto condStepConst = condStep ? dyn_cast<IntOp>(condStep->def) : nullptr;
  if (!condStepConst || condStepConst->value != step->value)
    return false;

  auto upper = dyn_cast<IntOp>(limit->def);
  if (!upper)
    return false;

  // Also consider the starting value.
  auto start = dyn_cast<IntOp>(loop->val(loop->getResultIndex(ind))->def);
  if (!start)
    return false;

  auto delta = upper->value - start->value;
  if (delta <= 0)
    return false;

  // Only unroll loops with iteration count <= 32.
  auto iterations = (delta + step->value - 1) / step->value;
  if (iterations > 32)
    return false;

  // Now do the real unrolling.
  Builder builder;
  builder.setBefore(loop);

  Builder::Map mapping;
  std::vector<Value*> out;
  for (unsigned i = 0; i < loop->getNumOperands(); i++)
    out.push_back(loop->val(i));

  for (int i = 0; i < iterations; i++) {
    auto cloned = builder.clone(loop, mapping);
    // Replace the references to the new loop with in-values, i.e. the previous out-values.
    for (unsigned i = 0; i < loop->getNumOperands(); i++)
      cloned->ret(i)->replaceAllUsesWith(out[i]);

    // New out-values are then mapping of old out-values.
    std::vector<Value*> nextOut(loop->getNumOperands());
    for (unsigned i = 0; i < loop->getNumOperands(); i++) {
      auto next = last->val(i + 1);
      auto index = loop->getResultIndex(next);
      if (index != loop->getNumResults()) {
        nextOut[i] = out[index];
        continue;
      }

      auto it = mapping.find(next);
      nextOut[i] = it == mapping.end() ? next : it->second;
    }
    out = std::move(nextOut);
    
    // Remove the final condition.
    auto bb = cloned->getRegion()->getFirstBlock();
    bb->getLastOp()->erase();
    bb->inlineBefore(loop);
    cloned->erase();
  }

  // Replace the references to the old loop with the final out values.
  for (unsigned i = 0; i < loop->getNumOperands(); i++)
    loop->ret(i)->replaceAllUsesWith(out[i]);
  // Erase the original loop, as it's no longer needed.
  loop->erase();
  return true;
}

}
