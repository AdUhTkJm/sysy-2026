#include "Common.h"

namespace opt {

declare_local_pass(SimplifyCFG,
  void removeDeadBlocks(FuncOp *op);
) {
  // Merge jumps when possible.
  auto region = func->getRegion();
  fixed(for (auto bb : *region) {
    auto last = dyn_cast<BOp>(bb->getLastOp());
    if (!last)
      continue;

    auto target = last->target;
    if (target->getNumOps() != 1)
      continue;

    auto b = dyn_cast<BOp>(target->getFirstOp());
    if (!b)
      continue;

    last->target = b->target;
    mark_changed;
  })

  removeDeadBlocks(func);
}

void SimplifyCFG::removeDeadBlocks(FuncOp *op) {
  auto region = op->getRegion();
  region->updatePreds();

  std::set<Block*> reachable { region->getFirstBlock() };
  std::vector<Block*> queue { region->getFirstBlock() };
  while (!queue.empty()) {
    auto bb = queue.back();
    queue.pop_back();
    for (auto x : bb->succs) {
      if (!reachable.count(x)) {
        reachable.insert(x);
        queue.push_back(x);
      }
    }
  }

  std::vector<Block*> unreachable;
  for (auto bb : *region) {
    if (reachable.count(bb))
      continue;

    unreachable.push_back(bb);
    bb->prepareErase();
  }

  for (auto bb : unreachable)
    bb->erase();
}

}
