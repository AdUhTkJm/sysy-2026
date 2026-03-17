#include "Common.h"
#include <algorithm>

namespace opt {

#define always_live_list(X) \
  X(ReturnOp) X(CallOp) X(StoreOp) X(ArrayStoreOp) X(FuncOp) X(IfOp) X(ExternCallOp)

// Note that the condition of IfOp is always needed;
// but we don't know whether the loop-carried variables will be needed.
#define non_removable_list(X) \
  always_live_list(X) X(FuncOp) X(DoWhileOp) X(ConditionOp) X(YieldOp)

#define isa_impl(Ty) \
  isa<Ty>(op) ||

static bool alwaysLive(Op *op) {
  return always_live_list(isa_impl) false;
}

static bool removable(Op *op) {
  return !(non_removable_list(isa_impl) false);
}

declare_local_pass(ADCE) {
  std::set<Value*> live;
  std::vector<Value*> worklist;
  // tie[k] = { ...v } means that if `k` is live, then all values in `v` are live.
  std::unordered_map<Value*, std::vector<Value*>> tie;

  walk(func, [&](Op *op) {
    if (alwaysLive(op)) {
      for (auto v : op->getOperands())
        worklist.push_back(v);
    }

    if (isa<ConditionOp>(op)) {
      auto outer = op->getParentOfType<DoWhileOp>();
      if (outer->getNumResults() + 1 != op->getNumOperands()) {
        outer->dump();
        op->dump();
        assert(false && "condition and while mismatch!");
      }

      for (unsigned i = 1; i < op->getNumOperands(); i++)
        tie[outer->ret(i - 1)].push_back(op->val(i));
      worklist.push_back(op->val(0));
    }

    if (isa<YieldOp>(op)) {
      auto outer = op->getParentOfType<IfOp>();
      if (outer->getNumResults() != op->getNumOperands()) {
        outer->dump();
        op->dump();
        assert(false && "yield and if mismatch!");
      }

      for (unsigned i = 0; i < op->getNumOperands(); i++)
        tie[outer->ret(i)].push_back(op->val(i));
    }
  });

  while (!worklist.empty()) {
    Value *v = worklist.back();
    worklist.pop_back();
    if (live.count(v))
      continue;
    live.insert(v);

    // For loops and branches, their results are independent from each other.
    // Hence we only add the corresponding value from yield/condition.
    if (isa<DoWhileOp>(v->def) || isa<IfOp>(v->def)) {
      if (isa<DoWhileOp>(v->def)) {
        auto index = v->def->getResultIndex(v);
        auto val = v->def->val(index);
        worklist.push_back(val);
      }

      const auto &tied = tie.at(v);
      std::copy(tied.begin(), tied.end(), std::back_inserter(worklist));
      continue;
    }

    for (auto val : v->def->getOperands()) {
      worklist.push_back(val);
    }
  }

  std::unordered_map<Op*, std::set<unsigned, std::greater<unsigned>>> deferred;

  walk(func, [&](Op *op) {
    if (removable(op) && std::all_of(op->getResults().begin(), op->getResults().end(), [&](Value *v) { return !live.count(v); })) {
      op->clearOperands();
      return;
    }

    auto nops = op->getNumOperands();
    if (isa<ConditionOp>(op) && nops > 0) {
      auto parent = op->getParentOfType<DoWhileOp>();
      for (unsigned i = nops - 1; i >= 1; i--) {
        if (live.count(parent->ret(i - 1)))
          continue;
        
        op->removeOperand(i);
        parent->removeOperand(i - 1);
        deferred[parent].insert(i - 1);
      }
      return;
    }

    if (isa<YieldOp>(op) && nops > 0) {
      auto parent = op->getParentOfType<IfOp>();
      for (unsigned i = nops; i--; ) {
        if (live.count(parent->ret(i)))
          continue;
        
        op->removeOperand(i);
        deferred[parent].insert(i);
      }
      return;
    }
  });

  // Now use-def chains are erased, and we can begin removal of results.
  // Here `v` is guaranteed to be from largest to smallest, so it's safe.
  for (const auto &[k, v] : deferred) {
    for (auto i : v)
      k->removeResult(i);
  }

  walk(func, [&](Op *op) {
    if (removable(op) && std::all_of(op->getResults().begin(), op->getResults().end(), [&](Value *v) { return !live.count(v); }))
      op->erase();
  });
}

}
