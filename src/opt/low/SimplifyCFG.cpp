#include "Common.h"

namespace opt {

static void rewire(Block *modif, Block *before, Block *after) {
  for (auto op : *modif) {
    auto phi = dyn_cast<PhiOp>(op);
    if (!phi)
      break;

    phi->replaceIncoming(before, after);
  }
}

declare_local_pass(SimplifyCFG,
  void removeDeadBlocks(FuncOp *op);
) {
  // Merge jumps when possible.
  auto region = func->getRegion();
  fixed(for (auto bb : *region) {
    if (bb->getNumOps() != 1)
      continue;

    auto last = dyn_cast<BOp>(bb->getLastOp());
    if (!last)
      continue;

    for (auto x : *region) {
      auto l = x->getLastOp();
      if (auto p = targetOf(l); p == bb)
        setTarget(l, last->target), rewire(last->target, bb, x), mark_changed;
      if (auto p = elseOf(l); p == bb)
        setElse(l, last->target), rewire(last->target, bb, x), mark_changed;
    }
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
