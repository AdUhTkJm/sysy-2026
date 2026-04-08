#include "Common.h"
#include <algorithm>

namespace opt {

Pass *makePure(ir::ModuleOp *module);

declare_pass(HighDCE,
  bool removeIfResults(IfOp *op);
  bool removeWhileResults(DoWhileOp *op);
) {
  makePure(module)->run();
  fixed(
    walk<Postorder>(module, [&](Op *op) {
      if (std::all_of(op->getResults().begin(), op->getResults().end(), [](Value *v){
        return !v->used();
      }) && !op->has<UnerasableAttr>() && !hasSideEffect(op)) {
        op->erase();
        mark_changed;
        return;
      }

      if (auto x = dyn_cast<IfOp>(op)) {
        __changed |= removeIfResults(x);
        return;
      }

      if (auto x = dyn_cast<DoWhileOp>(op)) {
        __changed |= removeWhileResults(x);
        return;
      }
    });
  );

  walk(module, [](Op *op) { op->remove<UnerasableAttr>(); });
}

bool HighDCE::removeIfResults(IfOp *op) {
  std::vector<int> unused;
  for (auto [i, result] : data::enumerate(op->getResults())) {
    if (!result->used())
      unused.push_back(i);
  }
  for (int i = (int) unused.size() - 1; i >= 0; i--) {
    op->removeResult(unused[i]);

    for (auto r : op->getRegions()) {
      auto yield = dyn_cast<YieldOp>(r->getLastOp());
      if (!yield)
        continue;

      yield->removeOperand(unused[i]);
    }
  }
  return unused.size();
}

bool HighDCE::removeWhileResults(DoWhileOp *op) {
  std::vector<int> unused;
  for (auto [i, result] : data::enumerate(op->getResults())) {
    if (!result->used())
      unused.push_back(i);
  }

  auto r = op->getRegion();
  for (int i = (int) unused.size() - 1; i >= 0; i--) {
    op->removeResult(unused[i]);
    op->removeOperand(unused[i]);

    auto cond = cast<ConditionOp>(r->getLastOp());
    // Don't forget `cond`'s first operand is the condition itself.
    cond->removeOperand(unused[i] + 1);
  }
  return unused.size();
}

}
