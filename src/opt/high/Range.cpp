#include "Common.h"

namespace opt {

RangeResult rangeResult;

declare_pass(Range,
  std::unordered_map<const Op*, data::Env> envs;
  data::Env cur;

  void runImpl(Region *region);
  void runOp(Op *op);

  void tidyUnreachable();
) {
  // Clear existing rangeResult.
  std::set<const data::Env::Data*> deleted;
  for (auto [k, v] : rangeResult) {
    if (!deleted.count(v.data)) {
      delete v.data;
      deleted.insert(v.data);
    }
  }
  rangeResult.clear();

  for (auto func : collectFunctions())
    runImpl(func->getRegion());

  for (auto [k, v] : envs)
    rangeResult.emplace(k, v);

  tidyUnreachable();
}

void Range::runImpl(Region *region) {
  assert(region->getNumBlocks() == 1);
  auto bb = region->getFirstBlock();

  for (auto op : *bb) {
    runOp(op);
    envs[op] = cur;
  }
}

#define arith_list(X) \
  X(AddIOp, +) X(SubIOp, -) X(MulIOp, *) X(DivIOp, /) X(ModIOp, %)

#define arith_impl(Ty, arith) \
  if (isa<Ty>(op)) { \
    cur[op->ret()] = cur[op->val(0)] arith cur[op->val(1)]; \
    return; \
  }

void Range::runOp(Op *op) {
  if (auto vi = dyn_cast<IntOp>(op)) {
    cur[op->ret()] = vi->value;
    return;
  }
  if (isa<SextOp>(op)) {
    cur[op->ret()] = cur[op->val()];
    return;
  }

  arith_list(arith_impl);

  if (isa<IfOp>(op)) {
    data::Env envl = cur.clone(), envr = cur.clone();
    // Look at the if-condition.
    auto cond = op->val()->def;
    if (isa<LtOp>(cond)) {
      auto l = cond->val(0), r = cond->val(1);
      auto vl = cur[l], vr = cur[r];
      // The if-branch is unreachable.
      if (vl.lo >= vr.hi) {
        op->set<UnreachableAttr>(0);
        runImpl(op->getRegion(1));
        return;
      }
      // The else-branch is unreachable.
      if (vl.hi < vr.lo) {
        op->set<UnreachableAttr>(1);
        runImpl(op->getRegion(0));
        return;
      }

      envl[l] = envl[l].intersect({ vl.lo, vr.hi - 1 });
      envl[r] = envl[r].intersect({ vl.lo, vr.hi - 1 });

      envr[l] = envr[l].intersect({ vr.lo, vl.hi });
      envr[r] = envr[r].intersect({ vr.lo, vl.hi });
    }

    auto old = cur; cur = envl;
    runImpl(op->getRegion(0));
    cur = envr;
    runImpl(op->getRegion(1));
    cur = old;
  }

  if (isa<DoWhileOp>(op)) {
    // Look at loop condition.
    auto region = op->getRegion();
    auto last = cast<ConditionOp>(region->getLastOp());
    auto cond = last->val()->def;
    if (isa<LtOp>(cond)) {
      // We suppose that the loop condition is `i + a < n`,
      // where `i` is the induction variable.
      auto ind = cond->val(0), lim = cond->val(1);

      // We have to check our assumption.
      auto index = cond->getOperandIndex(ind);
      if (index == cond->getNumOperands() || !isa<AddIOp>(ind->def))
        goto fail;

      auto l = ind->def->val(0), r = ind->def->val(1);
      auto loopvarIndex = op->getResultIndex(l);
      if (loopvarIndex == op->getNumResults()) {
        if ((loopvarIndex = op->getResultIndex(r)) == op->getNumResults())
          goto fail;
        std::swap(l, r);
      }

      auto start = op->val(loopvarIndex);

      // Now `l` is the induction variable.
      // It must be no less than `start` and less than `lim` inside the loop,
      // and be in [lim, lim + r) outside the loop.
      auto inloop = cur.clone(), outloop = cur.clone();
      auto vlim = cur[lim], vr = cur[r], vstart = cur[start];

      inloop[l] = inloop[l].intersect({ vstart.lo, vlim.hi - 1 });
      outloop[l] = outloop[l].intersect({ vlim.lo, vlim.hi + vr.hi - 1 });

      cur = inloop;
      runImpl(op->getRegion());
      cur = outloop;
      return;
    }

    fail:
    // Make no further assumptions.
    runImpl(op->getRegion());
  }
}

void Range::tidyUnreachable() {
  walk<Postorder>(module, [](Op *op) {
    if (!isa<IfOp>(op))
      return;

    auto unreachable = op->get<UnreachableAttr>();
    if (!unreachable)
      return;

    auto preserved = op->getRegion(!unreachable->region);
    auto yield = preserved->getLastOp();
    for (auto bb : *preserved)
      bb->inlineBefore(op);
    
    for (auto [i, v] : data::enumerate(op->getResults()))
      v->replaceAllUsesWith(yield->val(i));
    
    op->erase();
    yield->erase();
  });
}

}
