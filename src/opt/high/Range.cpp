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
        runImpl(op->getRegion(1));
        return;
      }
      // The else-branch is unreachable.
      if (vl.hi < vr.lo) {
        runImpl(op->getRegion(0));
        return;
      }

      envl[l] = envl[l].intersect({ vl.lo, vr.hi - 1 });
      envl[r] = envl[r].intersect({ vl.lo + 1, vr.hi });

      envr[l] = envr[l].intersect({ vr.lo, vl.hi });
      envr[r] = envr[r].intersect({ vr.lo, vl.hi });
    }

    auto old = cur; cur = envl;
    runImpl(op->getRegion(0));
    cur = envr;
    runImpl(op->getRegion(1));
    cur = old;

    // Check the yields.
    auto lastL = op->getRegion(0)->getLastOp(), lastR = op->getRegion(1)->getLastOp();
    for (unsigned i = 0; i < op->getNumResults(); i++) {
      if (!isa<YieldOp>(lastL)) {
        if (!isa<YieldOp>(lastR))
          break;

        std::swap(lastL, lastR);
      }

      if (isa<YieldOp>(lastR))
        cur[op->ret(i)] = envl[lastL->val(i)].join(envr[lastR->val(i)]);
      else
        cur[op->ret(i)] = envl[lastL->val(i)];
    }
  }

  if (isa<DoWhileOp>(op)) {
    // Look at loop condition.
    auto region = op->getRegion();
    auto last = cast<ConditionOp>(region->getLastOp());
    auto cond = last->val()->def;
    auto inloop = cur.clone(), outloop = cur.clone();

    // For loops too deep, don't do this.
    // This is mainly for efficiency: the algorithm is O(t^depth) for some constant `t`.
    // For 6-8 nested loops it just runs forever.
    int depth = 0;
    for (Op *runner = op; !isa<FuncOp>(runner); runner = runner->getParentOp()) {
      if (isa<DoWhileOp>(runner))
        depth++;
    }
    if (depth < 3) {
      for (unsigned i = 0; i < op->getNumResults(); i++) {
        if (op->ret(i)->type == i32)
          inloop[op->ret(i)] = inloop[op->val(i)];
      }
      auto old = cur;
      fixed(
      cur = inloop.clone();
      // Infer the range inside the loop.
      runImpl(op->getRegion());
      
      // Update the range based on condition result.
      for (unsigned i = 1; i < last->getNumOperands(); i++) {
        auto v = op->ret(i - 1);
        auto next = __index > 3
          ? inloop[v].widen(cur[last->val(i)])
          : inloop[v].join(cur[last->val(i)]);
        if (inloop[v] != next) {
          inloop[v] = next;
          mark_changed;
        }
      }
      delete cur.data;)

      // Further refinement.
      cur = old;
    }

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

    int unreachable = -1;
    auto cond = op->val()->def;
    auto it = rangeResult.find(op);
    if (it == rangeResult.end())
      return;

    if (isa<LtOp>(cond)) {
      auto l = cond->val(0), r = cond->val(1);
      auto vl = it->second[l], vr = it->second[r];
      // The if-branch is unreachable.
      if (vl.lo >= vr.hi)
        unreachable = 0;
      // The else-branch is unreachable.
      if (vl.hi < vr.lo)
        unreachable = 1;
    }
    if (unreachable == -1)
      return;

    auto preserved = op->getRegion(!unreachable);
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
