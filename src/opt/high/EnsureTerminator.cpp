#include "Common.h"

namespace opt {

static void eraseRedundant(Op *op) {
  for (auto r : op->getRegions()) {
    // At the very beginning, we should have only a single block.
    assert(r->getNumBlocks() == 1);
    OpList::iterator it;
    auto bb = r->getFirstBlock();
    for (it = bb->begin(); it != bb->end(); it++) {
      auto x = *it;
      if (isa<ReturnOp>(x) || isa<YieldOp>(x) || isa<ConditionOp>(x))
        break;
    }
    if (it != bb->end())
      it++;
    while (it != bb->end()) {
      auto next = it; next++;
      (*it)->erase();
      it = next;
    }
  }
}

declare_pass(EnsureTerminator) {
  auto ifs = collectOps<IfOp>();
  for (auto op : ifs)
    eraseRedundant(op);
  
  auto loops = collectOps<WhileOp>();
  for (auto op : loops)
    eraseRedundant(op);
}

}