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

void spill(Value *v, Region *region) {
  Builder builder;
  builder.setToStart(region);
  auto alloca = builder.create<AllocaOp>(Type::pointer(v->type));
  for (auto it = v->getUses().begin(); it != v->getUses().end();) {
    auto next = it; next++;
    Op *use = *it;
    builder.setBefore(use);
    auto ld = builder.create<LdrOp>(v->type)->with(alloca->ret());
    use->replaceOperand(v, ld->ret());
    it = next;
  }
}

}

namespace opt {

declare_pass(RegAlloc,
  void runImpl(Region *region, bool isLeaf);
  void markBlockConflict(Block *bb);
  bool allocate(Block *bb, bool isLeaf);
  void printModule();
  void clearState();

  std::unordered_map<Value*, Reg> tmpReg;
  // Interference map.
  std::unordered_map<Value*, std::set<Value*>> interf;
  // Values of readreg, or operands of writereg, or phis (mvs), are prioritzed.
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

  printModule();
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
    for (auto v : op->getResults()) {
      defined[v] = i;

      // Even though the op is not used, it still lives in the instruction that defines it.
      // Actually this should be eliminated with DCE, but we need to take care of it.
      if (!lastUsed.count(v))
        lastUsed[v] = i + 1;

      // Precolor.
      if (auto wr = dyn_cast<WriteRegOp>(op)) {
        tmpReg[v] = (Reg) wr->reg;
        priority[v] = 1;
      }
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
  // If they aren't defined in this block, then `defined[op]` will be zero, which is intended.
  for (auto op : bb->liveOut)
    lastUsed[op] = ops.size();

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

bool RegAlloc::allocate(Block *bb, bool isLeaf) {
  const Reg *order = isLeaf ? leafOrder : normalOrder;
  const Reg *orderf = isLeaf ? leafOrderf : normalOrderf;

  for (auto op : *bb) {
    for (auto v : op->getResults()) {
      if (tmpReg.count(v))
        continue;

      std::set<Reg> bad, unpreferred;

      for (auto z : interf[v]) {
        // In the whole function, `sp` and `zero` are read-only.
        if (tmpReg.count(z) && tmpReg[z] != sp && tmpReg[z] != xzr)
          bad.insert(tmpReg[z]);
      }

      if (isa<PhiOp>(op)) {
        // Dislike everything that might interfere with phi's operands.
        const auto &operands = phiOperand[v];
        for (auto x : operands) {
          for (auto v : interf[x]) {
            if (tmpReg.count(v) && tmpReg[v] != sp && tmpReg[v] != xzr)
              unpreferred.insert(tmpReg[v]);
          }
        }
      }

      if (prefer.count(v)) {
        auto ref = prefer[v];
        // Try to allocate the same register as `ref`.
        if (tmpReg.count(ref) && !bad.count(tmpReg[ref])) {
          tmpReg[v] = tmpReg[ref];
          continue;
        }
      }

      // See if there's any preferred registers.
      int preferred = -1;
      for (auto use : v->getUses()) {
        if (auto wr = dyn_cast<WriteRegOp>(use)) {
          int reg = wr->reg;
          if (!bad.count((Reg) reg)) {
            preferred = reg;
            break;
          }
        }
      }
      if (auto rd = dyn_cast<ReadRegOp>(op)) {
        int reg = rd->reg;
        if (!bad.count((Reg) reg))
          preferred = reg;
      }

      if (preferred != -1) {
        tmpReg[v] = (Reg) preferred;
        continue;
      }

      // Assign a register by enumerating.
      auto rn = regbank(v->type) == INT ? regcnt : regcntf;
      auto regs = regbank(v->type) == INT ? order : orderf;

      for (int i = 0; i < rn; i++) {
        if (!bad.count(regs[i])) {
          tmpReg[v] = (Reg) regs[i];
          break;
        }
      }

      // The value must be spilled.
      if (!tmpReg.count(v)) {
        spill(v, bb->getParentRegion());
        std::cout << "spilled value: " << v->def << "\n";
        return false;
      }
    }
  }

  return true;
}

void RegAlloc::printModule() {
}

void RegAlloc::clearState() {
  interf.clear();
  priority.clear();
  prefer.clear();
  phiOperand.clear();
  tmpReg.clear();
}

void RegAlloc::runImpl(Region *region, bool isLeaf) {
  region->updateLiveness();

  for (auto bb : region->getBlocks()) {
    bool success;
    do {
      clearState();
      markBlockConflict(bb);
      success = allocate(bb, isLeaf);
    } while (!success);
    assignment.insert(tmpReg.begin(), tmpReg.end());
  }
}

}
