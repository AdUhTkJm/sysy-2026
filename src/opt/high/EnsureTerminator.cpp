#include "Common.h"

namespace opt {

#define container_list(X) \
  X(IfOp) X(DoWhileOp) X(BreakOp) X(ContinueOp)

#define part_container_list(X) \
  X(BreakOp) X(ContinueOp)

#define container_name(Ty) \
  Ty##s

#define container_of(Ty) \
  std::set<Ty*> container_name(Ty);

#define loop_with(Ty, function) \
  for (auto it = container_name(Ty).begin(); it != container_name(Ty).end(); ) { \
    auto op = *it; \
    function(op); \
    it = container_name(Ty).upper_bound(op); \
  }

#define init(Ty) \
  { \
    auto ops = collectOps<Ty>(); \
    container_name(Ty) = std::set<Ty*>(ops.begin(), ops.end()); \
  }

#define container_empty(Ty) \
  container_name(Ty).empty() &&

declare_pass(EnsureTerminator,
  void removeContinue(ContinueOp *op);
  void removeBreak(BreakOp *op);
  void removeRedundant(Op *op);
  void erase(Op *op);

  container_list(container_of)
) {
  container_list(init)
  loop_with(IfOp, removeRedundant);
  loop_with(DoWhileOp, removeRedundant);

  do {
    part_container_list(init)
    loop_with(BreakOp, removeBreak);
    loop_with(ContinueOp, removeContinue);
    std::cout << "continue size: " << ContinueOps.size() << "\n";
  } while (!(part_container_list(container_empty) true));

  for_all(CondMarkerOp)
    op->erase();
}

#define erase_from(Ty) \
  if (auto x = dyn_cast<Ty>(op)) \
    container_name(Ty).erase(x);

// Update operation list whilst erasing. This is to avoid processing erased operations.
void EnsureTerminator::erase(Op *op) {
  container_list(erase_from);
  op->erase();
}

void EnsureTerminator::removeRedundant(Op *op) {
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
      erase(*it);
      it = next;
    }
  }
}

void EnsureTerminator::removeBreak(BreakOp *op) {
  (void) op; assert(false && "NYI");
}

void EnsureTerminator::removeContinue(ContinueOp *op) {
  auto parent = op->getParentOp();
  auto grandpa = parent->getParentBlock();

  if (isa<IfOp>(parent)) {
    // Essentially, move everything after this `if`
    // into the other branch.
    auto branch = op->getParentRegion();
    auto other = parent->getRegion(parent->getRegion(0) == branch);

    auto last = other->getLastOp();
    Builder builder;

    // These operantions terminate the other branch as well.
    // So the instructions afterwards will never be executed.
    // We remove them.
    if (auto cont = dyn_cast<ContinueOp>(last)) {
      for (auto x = parent->nextOp(); x && !isa<CondMarkerOp>(x);) {
        auto next = x->nextOp();
        x->clearOperands();
        x = next;
      }
      for (auto x = parent->nextOp(); x && !isa<CondMarkerOp>(x);) {
        auto next = x->nextOp();
        erase(x);
        x = next;
      }

      // Now both continues become normal yields.
      container_name(ContinueOp).erase(cont);
      container_name(ContinueOp).erase(op);
      
      builder.replace<YieldOp>(last);
      builder.replace<YieldOp>(op);
      return;
    }

    assert(isa<YieldOp>(last));
    last->erase();
    // Move everything before the condition evaluation into `other` branch.
    for (auto op = parent->nextOp(); op && !isa<CondMarkerOp>(op); ) {
      auto next = op->nextOp();
      op->moveToEnd(other->getFirstBlock());
      op = next;
    }
    builder.setToEnd(other);
    builder.create<YieldOp>();

    // Move the continue to the outer layer, and supply a YieldOp.
    builder.setBefore(op);
    builder.create<YieldOp>();
    op->moveToEnd(grandpa);
    return;
  }

  // This will just skip rest of the loop.
  if (isa<DoWhileOp>(parent)) {
    for (Op *x = op; x && !isa<CondMarkerOp>(x);) {
      auto next = x->nextOp();
      x->clearOperands();
      x = next;
    }
    for (Op *x = op; x && !isa<CondMarkerOp>(x);) {
      auto next = x->nextOp();
      erase(x);
      x = next;
    }
  }
}

}
