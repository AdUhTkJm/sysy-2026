#include "Common.h"

namespace opt {

declare_local_pass(RedundantLoad,
  using Address = std::vector<Value *>;
  using Memory = std::map<Address, Value *>;
  static constexpr unsigned loopFixpointLimit = 5;

  std::optional<Address> addressOf(Op *op) const;
  Value *stored(Op *op) const;

  Memory runOnRegion(Region *region, const Memory &entry) const;
  Memory runOnOp(Op *op, const Memory &entry) const;
  Memory runOnIf(IfOp *br, const Memory &entry) const;
  Memory runOnLoop(DoWhileOp *loop, const Memory &entry) const;

  Memory rewriteRegion(Region *region, const Memory &entry);
  void rewriteOp(Op *op, Memory &state);

  // If the values from `ifso` and `ifnot` are on both yields, then it can be substituted with if-result.
  // Otherwise the merge fails.
  std::optional<Value*> mergeIfValue(IfOp *br, Value *ifso, Value *ifnot) const;
  // Similarly, if the value is on the condition, then it can be projected as the loop variable.
  std::optional<Value*> projectLoopValue(DoWhileOp *loop, Value *v) const;

  static Memory intersect(const Memory &lhs, const Memory &rhs);
) {
  rewriteRegion(func->getRegion(), {});
}

std::optional<RedundantLoad::Address> RedundantLoad::addressOf(Op *op) const {
  if (isa<LoadOp>(op) || isa<StoreOp>(op))
    return Address { op->val(0) };

  if (isa<ArrayLoadOp>(op))
    return Address(op->getOperands().begin(), op->getOperands().end());

  if (isa<ArrayStoreOp>(op))
    return Address(op->getOperands().begin(), op->getOperands().end() - 1);

  return std::nullopt;
}

Value *RedundantLoad::stored(Op *op) const {
  if (isa<StoreOp>(op))
    return op->val(1);
  if (isa<ArrayStoreOp>(op))
    return op->val(op->getNumOperands() - 1);
  return nullptr;
}

RedundantLoad::Memory RedundantLoad::intersect(const Memory &lhs, const Memory &rhs) {
  Memory out;
  for (const auto &[addr, value] : lhs) {
    auto it = rhs.find(addr);
    if (it != rhs.end() && it->second == value)
      out[addr] = value;
  }
  return out;
}

std::optional<Value*> RedundantLoad::mergeIfValue(IfOp *br, Value *ifso, Value *ifnot) const {
  if (ifso == ifnot && !ifso->def->inside(br))
    return ifso;

  auto yield1 = dyn_cast<YieldOp>(br->getRegion(0)->getLastOp());
  auto yield2 = dyn_cast<YieldOp>(br->getRegion(1)->getLastOp());
  if (!yield1 || !yield2)
    return std::nullopt;

  for (unsigned i = 0; i < yield1->getNumOperands(); i++) {
    if (yield1->val(i) == ifso && yield2->val(i) == ifnot)
      return br->ret(i);
  }
  return std::nullopt;
}

std::optional<Value*> RedundantLoad::projectLoopValue(DoWhileOp *loop, Value *v) const {
  if (!v->def->inside(loop))
    return v;

  if (loop->getNumRegions() != 1)
    return std::nullopt;

  auto cond = dyn_cast<ConditionOp>(loop->getRegion()->getLastOp());
  if (!cond)
    return std::nullopt;

  for (unsigned i = 1; i < cond->getNumOperands(); i++) {
    if (cond->val(i) == v)
      return loop->ret(i - 1);
  }
  return std::nullopt;
}

RedundantLoad::Memory RedundantLoad::runOnIf(IfOp *br, const Memory &entry) const {
  auto ifso  = runOnRegion(br->getRegion(0), entry);
  auto ifnot = runOnRegion(br->getRegion(1), entry);

  Memory out;
  for (const auto &[addr, val] : ifso) {
    auto it = ifnot.find(addr);
    if (it == ifnot.end())
      continue;
    if (auto merged = mergeIfValue(br, val, it->second))
      out[addr] = *merged;
  }
  return out;
}

RedundantLoad::Memory RedundantLoad::runOnLoop(DoWhileOp *loop, const Memory &entry) const {
  if (loop->getNumRegions() != 1)
    return {};

  Memory loopEntry = entry;
  unsigned i = 0;
  for (; i < loopFixpointLimit; i++) {
    auto exit = runOnRegion(loop->getRegion(), loopEntry);
    auto next = intersect(entry, exit);
    if (next == loopEntry)
      break;
    loopEntry = std::move(next);
  }
  // Does not converge.
  if (i == loopFixpointLimit)
    return {};

  Memory out, exit = runOnRegion(loop->getRegion(), loopEntry);
  for (const auto &[addr, value] : exit) {
    if (auto projected = projectLoopValue(loop, value))
      out[addr] = *projected;
  }
  return out;
}

RedundantLoad::Memory RedundantLoad::runOnOp(Op *op, const Memory &entry) const {
  auto state = entry;

  if (isa<IfOp>(op))
    return runOnIf(cast<IfOp>(op), entry);
  if (isa<DoWhileOp>(op))
    return runOnLoop(cast<DoWhileOp>(op), entry);

  if (auto addr = addressOf(op)) {
    if (isa<LoadOp>(op) || isa<ArrayLoadOp>(op)) {
      auto it = state.find(*addr);
      if (it == state.end() || it->second->type != op->ret()->type)
        state[*addr] = op->ret();
      return state;
    }

    if (auto value = stored(op))
      state[*addr] = value;
    return state;
  }

  if (isa<CallOp>(op) || isa<ExternCallOp>(op) || hasSideEffect(op))
    return {};

  return state;
}

RedundantLoad::Memory RedundantLoad::runOnRegion(Region *region, const Memory &entry) const {
  Memory state = entry;
  for (auto op : *region->getFirstBlock() )
    state = runOnOp(op, state);
  return state;
}

RedundantLoad::Memory RedundantLoad::rewriteRegion(Region *region, const Memory &entry) {
  Memory state = entry;
  auto bb = region->getFirstBlock();
  for (auto it = bb->begin(); it != bb->end();) {
    auto op = *it; ++it;
    rewriteOp(op, state);
  }
  return state;
}

void RedundantLoad::rewriteOp(Op *op, Memory &state) {
  if (auto br = dyn_cast<IfOp>(op)) {
    auto entry = state;
    auto ifso  = rewriteRegion(br->getRegion(0), entry);
    auto ifnot = rewriteRegion(br->getRegion(1), entry);

    Memory out;
    for (const auto &[addr, val] : ifso) {
      auto it = ifnot.find(addr);
      if (it == ifnot.end())
        continue;
      auto merged = mergeIfValue(br, val, it->second);
      if (merged)
        out[addr] = *merged;
    }
    state = std::move(out);
    return;
  }

  if (auto loop = dyn_cast<DoWhileOp>(op)) {
    if (loop->getNumRegions() != 1) {
      for (auto region : loop->getRegions())
        rewriteRegion(region, {});
      state.clear();
      return;
    }

    Memory loopEntry = state;
    unsigned i = 0;
    for (; i < loopFixpointLimit; i++) {
      auto exit = runOnRegion(loop->getRegion(), loopEntry);
      auto next = intersect(state, exit);
      if (next == loopEntry)
        break;
      loopEntry = std::move(next);
    }
    if (i == loopFixpointLimit) {
      rewriteRegion(loop->getRegion(), {});
      state.clear();
      return;
    }

    auto exit = rewriteRegion(loop->getRegion(), loopEntry);
    Memory out;
    for (const auto &[addr, value] : exit) {
      auto projected = projectLoopValue(loop, value);
      if (projected)
        out[addr] = *projected;
    }
    state = std::move(out);
    return;
  }

  auto addr = addressOf(op);
  if (!addr) {
    if (isa<CallOp>(op) || isa<ExternCallOp>(op) || hasSideEffect(op))
      state.clear();
    return;
  }

  if (isa<LoadOp>(op) || isa<ArrayLoadOp>(op)) {
    auto it = state.find(*addr);
    if (it != state.end() && it->second->type == op->ret()->type) {
      op->ret()->replaceAllUsesWith(it->second);
      op->erase();
      return;
    }

    state[*addr] = op->ret();
    return;
  }

  if (auto value = stored(op))
    state[*addr] = value;
  else
    state.erase(*addr);
}

}
