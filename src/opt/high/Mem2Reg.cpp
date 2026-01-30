#include "Common.h"

namespace opt {

declare_local_pass(Mem2Reg,
  bool isLiftable(Value *value);
  void recurse(Op *op);
  std::map<Value*, Value*> valmap;
  std::vector<Op*> toremove;
) {
  std::cout << func << "\n";
  for (auto bb : *func->getRegion()) {
    for (auto op : *bb)
      recurse(op);
  }
  for (auto x : toremove)
    x->erase();
  toremove.clear();
  std::cout << "done\n";
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
    for (auto [k, v] : left) {
      auto it = right.find(k);
      // The value is undefined for the right branch, so if we use it from
      // here then this is undefined behaviour.
      // So we're permitted to do anything, e.g. assume it is the same as
      // the left branch.
      //
      // Moreover, when the values are the same, we know nothing happened.
      if (it == right.end() || it->second == v) {
        valmap[k] = v;
        continue;
      }

      // The values might be different on the two sides.
      // In that case, we add them to the final yield operation.
      yl->pushOperand(v);
      yr->pushOperand(it->second);
      auto newv = op->pushResult(v->type);
      valmap[k] = newv;
    }
    // Add things defined in `r` but not defined in `l`. From a similar reasoning
    // we can do this.
    for (auto [k, v] : right) {
      if (!left.count(k))
        valmap[k] = v;
    }
    return;
  }

  if (isa<DoWhileOp>(op)) {
    // For a do-while op, every store will become the block-argument.
  }
}

}
