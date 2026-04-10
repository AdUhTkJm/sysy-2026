#include "Common.h"
#include <algorithm>

namespace {

using namespace opt;

struct Event {
  int timestamp;
  bool start;
  Value *value;
};

bool conflict(const Type *a, const Type *b) {
  return regbank(a) == regbank(b);
}

void spill(Value *v, Region *region, int i) {
  assert(i < 3);
  Builder builder;
  builder.setToStart(region);
  auto alloca = builder.create<AllocaOp>(Type::pointer(v->type));
  assignment[v] = (Reg) (unsigned long) alloca;
}

[[gnu::unused]] void dumpInterf(Region *region, const std::unordered_map<Value*, std::set<Value*>> &interf) {
  std::cerr << region;
  std::cerr << "\n\n===== interference graph =====\n\n";
  for (auto [k, v] : interf) {
    std::cerr << k << ": ";
    for (auto val : v)
      std::cerr << val << " ";
    std::cerr << "\n";
  }
}

[[gnu::unused]] void dumpAssignment(Region *region, const std::unordered_map<Value*, Reg> &assignment) {
  std::cerr << region;
  std::cerr << "\n\n===== assignment =====\n\n";
  for (auto [k, v] : assignment) {
    std::cerr << k << " = " << regname(v) << "\n";
  }
}

}

namespace opt {

declare_pass(RegAlloc,
  void runImpl(Region *region, bool isLeaf);
  void markBlockConflict(Block *bb);
  void allocate(Block *bb, bool isLeaf);
  void clearState();
  void checkLegality(Region *region) const;

  // Interference map.
  std::unordered_map<Value*, std::set<Value*>> interf;
  // Values of readreg, or operands of writereg, or phis (mvs), are prioritized.
  std::unordered_map<Value*, int> priority;
  // The `key` is preferred to have the same value as `value`.
  std::unordered_map<Value*, Value*> prefer;
  // Maps a phi's return value to its operands.
  std::unordered_map<Value*, std::vector<Value*>> phiOperand;
) {
  for (auto x : collectFunctions()) {
    auto calls = collectOps<CallOp>(x);
    runImpl(x->getRegion(), calls.empty());
  }
}

void RegAlloc::markBlockConflict(Block *bb) {
  int currentPriority = 2;
  // Scan through the block and see the place where the value's last used.
  std::map<Value*, int> lastUsed, defined;
  const auto &ops = bb->getOps();
  auto it = ops.end();
  for (int i = (int) ops.size() - 1; i >= 0; i--) {
    auto op = *--it;

    for (auto v : op->getOperands()) {
      if (!lastUsed.count(v))
        lastUsed[v] = i;
    }
    if (isa<WriteRegOp>(op))
      priority[op->val()] = 1;

    for (auto v : op->getResults()) {
      defined[v] = i;

      // Even though the op is not used, it still lives in the instruction that defines it.
      // Actually this should be eliminated with DCE, but we need to take care of it.
      if (!lastUsed.count(v))
        lastUsed[v] = i + 1;
      
      if (isa<ReadRegOp>(op))
        priority[v] = 1;
      
      // This doesn't need any load/store when spilled.
      if (auto movi = dyn_cast<MovIOp>(op); movi && movi->value <= 32767 && movi->value >= -32768)
        priority[v] = -2;
      
      if (isa<PhiOp>(op)) {
        priority[v] = currentPriority + 1;
        for (auto x : op->getOperands()) {
          priority[x] = currentPriority;
          prefer[x] = v;
          phiOperand[v].push_back(x);
        }
        currentPriority += 2;
      }
    }
  }

  // For all liveOuts, they are last-used at place size().
  // If they aren't defined in this block, then `defined[v]` will be zero, which is intended.
  for (auto v : bb->liveOut)
    lastUsed[v] = ops.size();

  // We use event-driven approach to optimize it into O(n log n + E).
  std::vector<Event> events;
  for (auto [value, time] : lastUsed) {
    // Don't push empty live range. It's not handled properly.
    if (defined[value] == time)
      continue;
    
    events.push_back(Event { defined[value], true, value });
    events.push_back(Event { time, false, value });
  }

  // Sort with ascending time (i.e. instruction count).
  std::sort(events.begin(), events.end(), [](Event a, Event b) {
    // For the same timestamp, we first set END events as inactive, then deal with START events.
    return a.timestamp == b.timestamp ? (!a.start && b.start) : a.timestamp < b.timestamp;
  });

  std::set<Value*> active;
  for (const auto& event : events) {
    auto value = event.value;
    // These will be relocated later. They don't participate in inteference calculation.
    if (isa<AllocaOp>(value->def))
      continue;
    if (event.start) {
      for (Value* v : active) {
        // FP and int are using different registers.
        // However, they use the same stack frame.
        if (!conflict(v->type, value->type))
          continue;

        interf[value].insert(v);
        interf[v].insert(value);
      }
      active.insert(value);
    } else
      active.erase(value);
  }
}

void RegAlloc::allocate(Block *bb, bool isLeaf) {
  const Reg *order = isLeaf ? leafOrder : normalOrder;
  const Reg *orderf = isLeaf ? leafOrderf : normalOrderf;

  for (auto op : *bb) {
    for (auto v : op->getResults()) {
      if (assignment.count(v))
        continue;

      std::set<Reg> bad, unpreferred;

      for (auto z : interf[v]) {
        // In the whole function, `sp` and `zero` are read-only.
        if (assignment.count(z) && assignment[z] != sp && assignment[z] != xzr)
          bad.insert(assignment[z]);
      }

      if (isa<PhiOp>(op)) {
        // Dislike everything that might interfere with phi's operands.
        const auto &operands = phiOperand[v];
        for (auto x : operands) {
          for (auto v : interf[x]) {
            if (assignment.count(v) && assignment[v] != sp && assignment[v] != xzr)
              unpreferred.insert(assignment[v]);
          }
        }
      }

      if (prefer.count(v)) {
        auto ref = prefer[v];
        // Try to allocate the same register as `ref`.
        if (assignment.count(ref) && !bad.count(assignment[ref])) {
          assignment[v] = assignment[ref];
          continue;
        }
      }

      // See if there's any preferred register.
      unsigned long preferred = -1;
      for (auto use : v->getUses()) {
        if (auto wr = dyn_cast<WriteRegOp>(use)) {
          auto reg = wr->reg;
          if (!bad.count((Reg) reg)) {
            preferred = reg;
            break;
          }
        }
      }
      if (auto rd = dyn_cast<ReadRegOp>(op)) {
        auto reg = rd->reg;
        if (!bad.count((Reg) reg))
          preferred = reg;
      }

      if (preferred != -1ull) {
        assignment[v] = (Reg) preferred;
        continue;
      }

      // Assign a register by enumerating.
      auto rn = regbank(v->type) == INT ? regcnt : regcntf;
      auto regs = regbank(v->type) == INT ? order : orderf;

      for (int i = 0; i < rn; i++) {
        if (!bad.count(regs[i])) {
          assignment[v] = (Reg) regs[i];
          break;
        }
      }

      // The value must be spilled.
      if (!assignment.count(v))
        spill(v, bb->getParentRegion(), op->getResultIndex(v));
    }
  }
}

void RegAlloc::clearState() {
  interf.clear();
  priority.clear();
  prefer.clear();
  phiOperand.clear();
}

void RegAlloc::runImpl(Region *region, bool isLeaf) {
  region->updateLiveness();

  clearState();
  for (auto bb : region->getBlocks())
    markBlockConflict(bb);
  for (auto bb : region->getBlocks())
    allocate(bb, isLeaf);
  for_all(BlOp, region->getParentOp())
    op->clearResults();
}

}
