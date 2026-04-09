#include "Common.h"
#include <cstring>

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

  auto funcs = collectFunctions();
  std::unordered_map<FuncOp*, std::vector<data::Interval>> ranges;
  for (auto func : funcs) {
    auto &vec = ranges[func];
    vec.resize(func->getNumResults() - 1);
    for (auto &x : vec)
      x = data::Interval::empty;
  }
  for (auto func : funcs)
    runImpl(func->getRegion());

  fixed(
  // Now consider cross-function ranges.
  for_all(CallOp) {
    auto fn = cast<FuncOp>(op->val()->def);
    for (unsigned i = 1; i < op->getNumOperands(); i++) {
      auto it = envs.find(op);
      if (it == envs.end())
        continue;

      auto old = ranges[fn][i - 1];
      // Apply widening to avoid infinite (or too long) loops.
      auto joined = __index > 3
        ? old.widen((it->second)[op->val(i)])
        : old.join ((it->second)[op->val(i)]);

      if (old != joined) {
        mark_changed;
        ranges[fn][i - 1] = joined;
      }
    }
  }
  for (auto func : funcs) {
    cur = data::Env();
    for (unsigned i = 1; i < func->getNumResults(); i++)
      cur[func->ret(i)] = ranges[func][i - 1];
    runImpl(func->getRegion());
  }
  )

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
    auto inloop = cur.clone(), outloop = cur.clone();
    if (isa<LtOp>(cond)) {
      // We suppose that the loop condition is `i + a < n`,
      // where `i` is the induction variable.
      auto ind = cond->val(0), lim = cond->val(1);

      // We have to check our assumption.
      auto index = cond->getOperandIndex(ind);
      if (index == cond->getNumOperands() || !isa<AddIOp>(ind->def))
        goto next;

      auto l = ind->def->val(0), r = ind->def->val(1);
      auto loopvarIndex = op->getResultIndex(l);
      if (loopvarIndex == op->getNumResults()) {
        if ((loopvarIndex = op->getResultIndex(r)) == op->getNumResults())
          goto next;
        std::swap(l, r);
      }

      auto start = op->val(loopvarIndex);

      // Now `l` is the induction variable.
      // It must be no less than `start` and less than `lim` inside the loop,
      // and be in [lim, lim + r) outside the loop.
      auto vlim = cur[lim], vr = cur[r], vstart = cur[start];

      inloop[l] = inloop[l].intersect({ vstart.lo, vlim.hi - 1 });
      auto max = long(vlim.hi - 1) + vr.hi;
      outloop[l] = outloop[l].intersect({ vlim.lo, max > INT_MAX ? INT_MAX : int(max) });
    }

    // Look at induction variables.
    next:
    for (unsigned i = 1; i < last->getNumOperands(); i++) {
      auto upd = last->val(i)->def;
      auto ind = op->ret(i - 1);

      // If this is monotonically increasing, then we know the lower bound.
      if (isa<AddIOp>(upd)) {
        auto l = upd->val(0), r = upd->val(1);
        if (r == ind)
          std::swap(l, r);

        auto vr = cur[r];
        if (vr.lo >= 0) {
          auto start = cur[op->val(i - 1)];
          inloop[l] = inloop[l].intersect({ start.lo, INT_MAX });
          outloop[l] = outloop[l].intersect({ start.lo, INT_MAX });
        }
      }
    }

    cur = inloop;
    runImpl(op->getRegion());
    cur = outloop;
    return;
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
