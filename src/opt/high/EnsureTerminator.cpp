#include "Common.h"

namespace opt {

#define terminator_container_list(X) \
  X(BreakOp) X(ContinueOp)

#define region_container_list(X) \
  X(IfOp) X(DoWhileOp)

#define container_list(X) \
  terminator_container_list(X) region_container_list(X)

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

#define loop_and_mark_changed(Ty, function) \
  for (auto it = container_name(Ty).begin(); it != container_name(Ty).end(); ) { \
    auto op = *it; \
    mark_changed_if(function(op)); \
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

  bool canonicalizeReturn(IfOp *op);
  bool canonicalizeReturn(DoWhileOp *op);
  void erase(Op *op);

  container_list(container_of)
) {
  container_list(init)
  loop_with(IfOp, removeRedundant);
  loop_with(DoWhileOp, removeRedundant);

  do {
    terminator_container_list(init)
    loop_with(BreakOp, removeBreak);
    loop_with(ContinueOp, removeContinue);
  } while (!(terminator_container_list(container_empty) true));

  fixed(
    loop_and_mark_changed(IfOp, canonicalizeReturn);
    loop_and_mark_changed(DoWhileOp, canonicalizeReturn);
  );
  
  loop_with(IfOp, removeRedundant);
  loop_with(DoWhileOp, removeRedundant);

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
      if (isTerminator(x) || isa<ConditionOp>(x))
        break;
    }
    assert(it != bb->end());
    it++;
    while (it != bb->end()) {
      auto next = it; next++;
      erase(*it);
      it = next;
    }
  }
}

void EnsureTerminator::removeBreak(BreakOp *op) {
  Builder builder;
  auto loop = op->getParentOfType<DoWhileOp>();

  // Create a new bool value as loop condition, and set it to 1.
  builder.setToStart(loop->getParentBlock());
  auto var = builder.create<AllocaOp>(Type::pointer(i32));
  auto one = builder.createInt(1);
  builder.create<StoreOp>()->with(var->ret(), one->ret());

  // Make sure the variable is included for the loop condition.
  auto cond = condition_of(loop);
  auto val = cond->val();

  builder.setBefore(cond);
  auto ld = builder.create<LoadOp>(i32)->with(var->ret());
  auto andop = builder.create<AndIOp>(i32)->with(val, ld->ret());
  cond->setOperand(0, andop->ret());

  // Before the break, we set the bool variable to zero.
  builder.setBefore(op);
  auto zero = builder.createInt(0);
  builder.create<StoreOp>()->with(var->ret(), zero->ret());

  // Replace the break with continue since it skips rest of the block.
  builder.rename<ContinueOp>(op);
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

    // These operations terminate the other branch as well.
    // So the instructions afterwards will never be executed.
    // We remove them.
    if (isa<ContinueOp>(last) || isa<ReturnOp>(last) || isa<BreakOp>(last)) {
      for (auto x = parent->nextOp(); x && !isa<CondMarkerOp>(x) && !isTerminator(x);) {
        auto next = x->nextOp();
        x->clearOperands();
        x = next;
      }
      for (auto x = parent->nextOp(); x && !isa<CondMarkerOp>(x) && !isTerminator(x);) {
        auto next = x->nextOp();
        erase(x);
        x = next;
      }

      // Now both continues become normal yields.
      {
        auto op = last;
        container_list(erase_from);
      }
      container_name(ContinueOp).erase(op);

      if (isa<ContinueOp>(last))
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

bool EnsureTerminator::canonicalizeReturn(IfOp *op) {
  auto l = op->getRegion(0), r = op->getRegion(1);
  auto lastL = l->getLastOp(), lastR = r->getLastOp();
  bool rl = isa<ReturnOp>(lastL), rr = isa<ReturnOp>(lastR);
  if (rl && rr) {
    // Both ends are returns, so we can remove everything till the CondMarker,
    // if that exists.
    for (Op *x = op->nextOp(); x && !isa<CondMarkerOp>(x);) {
      auto next = x->nextOp();
      x->clearOperands();
      x = next;
    }
    for (Op *x = op->nextOp(); x && !isa<CondMarkerOp>(x);) {
      auto next = x->nextOp();
      erase(x);
      x = next;
    }

    // Replace both ends with yield, and put return to the outer region.
    Builder builder;
    assert(lastL->getNumOperands() == lastR->getNumOperands());
    auto q = builder.rename<YieldOp>(lastL);
    builder.rename<YieldOp>(lastR);

    builder.setAfter(op);
    auto ret = builder.create<ReturnOp>();
    for (unsigned i = 0; i < q->getNumOperands(); i++) {
      auto v = op->pushResult(q->val(i)->type);
      ret->pushOperand(v);
    }
    return true;
  }

  if (!rr && !rl)
    return false;

  if (rr)
    std::swap(l, r);

  // Now `l` has a ReturnOp at the end, and `r` does not.
  // We move out everything in `r`.
  auto bb = r->getFirstBlock();
  for (Op *x = bb->getFirstOp(), *prev = op; x && !isa<YieldOp>(x);) {
    auto next = x->nextOp();
    x->moveAfter(prev);
    prev = x;
    x = next;
  }
  // Note that even though we changed, we haven't produced a new ReturnOp anywhere.
  // Therefore this change shouldn't cause a new round of loop.
  return false;
}

bool EnsureTerminator::canonicalizeReturn(DoWhileOp *op) {
  // If the return is directly put inside a while, then this means
  // the while can be completely removed: it will only be executed once.
  auto bb = op->getRegion()->getFirstBlock();
  bool hasReturn = false;
  for (auto x : *bb) {
    if (isa<ReturnOp>(x)) {
      hasReturn = true;
      break;
    }
  }
  if (!hasReturn)
    return false;

  for (auto x = bb->getFirstOp(); x && !isa<CondMarkerOp>(x) && !isa<ReturnOp>(x);) {
    auto next = x->nextOp();
    x->moveBefore(op);
    x = next;
  }
  erase(op);
  return true;
}

}
