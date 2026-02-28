#include "Common.h"

namespace opt {

declare_local_pass(Mem2Reg,
  bool isLiftable(Value *value);
  void recurse(Op *op);
  std::map<Value*, Value*> valmap;
  std::vector<Op*> toremove;
) {
  for (auto bb : *func->getRegion()) {
    for (auto op : *bb)
      recurse(op);
  }
  for (auto x : toremove)
    x->erase();
  toremove.clear();
}

bool Mem2Reg::isLiftable(Value *value) {
  return value->def && isa<AllocaOp>(value->def) && !value->def->get<DimAttr>();
}

void Mem2Reg::recurse(Op *op) {
  if (isa<StoreOp>(op)) {
    auto addr = op->val(0), val = op->val(1);
    if (isLiftable(addr)) {
      valmap[addr] = val;
      toremove.push_back(op);
    }
    return;
  }

  if (isa<LoadOp>(op)) {
    auto addr = op->val();
    auto ret = op->ret();
    if (!isLiftable(addr))
      return;

    if (auto it = valmap.find(addr); it != valmap.end())
      ret->replaceAllUsesWith(it->second);
    else {
      Builder builder(op);
      ret->replaceAllUsesWith(builder.create<UndefOp>(ret->type)->ret());
    }

    toremove.push_back(op);
    return;
  }

  if (isa<IfOp>(op)) {
    Region *l = op->getRegion(0), *r = op->getRegion(1);
    auto before = valmap;
    for (auto bb : *l) {
      for (auto op : *bb)
        recurse(op);
    }
    auto left = std::move(valmap);
    valmap = std::move(before);
    for (auto bb : *r) {
      for (auto op : *bb)
        recurse(op);
    }
    auto right = std::move(valmap);

    // If either branch returns, then we don't care about the values from it.
    if (isa<ReturnOp>(l->getLastOp())) {
      valmap = std::move(right);
      return;
    }
    if (isa<ReturnOp>(r->getLastOp())) {
      valmap = std::move(left);
      return;
    }

    // Now both branches don't return. They must terminate with a yield.
    auto yl = cast<YieldOp>(l->getLastOp()), yr = cast<YieldOp>(r->getLastOp());
    valmap.clear();
    Builder builder;
    for (auto [k, v] : left) {
      auto it = right.find(k);
      // When the values are the same, we know nothing happened.
      if (it != right.end() && it->second == v) {
        valmap[k] = v;
        continue;
      }

      // The values might be different on the two sides.
      // In that case, we add them to the final yield operation.
      auto w = it == right.end() ? (builder.setBefore(yr), builder.create<UndefOp>(v->type)->ret()) : it->second;
      yl->pushOperand(v);
      yr->pushOperand(w);
      valmap[k] = op->pushResult(v->type);
    }
    // Add things defined in `r` but not defined in `l`.
    for (auto [k, v] : right) {
      if (!left.count(k)) {
        builder.setBefore(yl);
        auto w = builder.create<UndefOp>(v->type)->ret();
        yl->pushOperand(w);
        yr->pushOperand(v);
        valmap[k] = op->pushResult(v->type);
      }
    }
    return;
  }

  // A do-while op will have its arguments as the initial arguments, and its results
  // both as loop variables and as final returned value.
  if (isa<DoWhileOp>(op)) {
    auto before = valmap;
    // All stores in the op might result in a dependency.
    auto stores = collectOps<StoreOp>(op);
    std::set<Value*> addrs;
    for (auto x : stores) {
      auto addr = x->val(0);
      if (!isLiftable(addr))
        continue;
      addrs.insert(addr);
    }

    auto r = op->getRegion();
    auto cond = cast<ConditionOp>(r->getLastOp());
    decltype(valmap) loopret;
    for (auto x : addrs) {
      auto it = before.find(x);
      Value *v;
      if (it == before.end()) {
        Builder builder(op);
        v = builder.create<UndefOp>(x->type->pointee())->ret();
      } else
        v = it->second;
      op->pushOperand(v);
      auto ret = op->pushResult(v->type);
      valmap[x] = loopret[x] = ret;
    }

    for (auto bb : *r) {
      for (auto op : *bb)
        recurse(op);
    }

    for (auto x : addrs) {
      auto it = valmap.find(x);
      Value *v;
      if (it == before.end()) {
        Builder builder(op);
        v = builder.create<UndefOp>(x->type->pointee())->ret();
      } else
        v = it->second;
      cond->pushOperand(v);
      // Restore valmap such that later operations refer to the loop result.
      valmap[x] = loopret[x];
    }
    return;
  }
}

}
