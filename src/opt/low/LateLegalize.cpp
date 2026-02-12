#include "Common.h"
#include "../../ir/Matcher.h"
#include <algorithm>

using namespace match;

namespace opt {

int asmSize(const AllocaOp *op) {
  auto sz = asmSize(op->ret()->type->pointee());
  if (auto dim = op->get<DimAttr>())
    sz *= dim->size();
  return sz;
}

static Rule rules[] = {
  Rule("(str (addxi base 'a) val 'b)") >> "(str base val (!add 'a 'b))",
  Rule("(ldr:T (addxi base 'a) 'b)") >> "(ldr:T base (!add 'a 'b))",
};

#define cvt_impl(Before, After) \
  if (auto br = dyn_cast<Before>(op)) { \
    auto other = br->other; \
    auto renamed = builder.rename<After>(op); \
    renamed->target = other; \
    renamed->other = nullptr; \
    return renamed; \
  }

#define cvt(A, B) cvt_impl(A, B) cvt_impl(B, A)

static Op *flip(Op *op) {
  Builder builder;
  cvt(CbzOp, CbnzOp)
  cvt(BeqOp, BneOp)
  cvt(BltOp, BgeOp)
  cvt(BleOp, BgtOp)
  assert(false && "should only flip branches!");
}

declare_local_pass(LateLegalize,
  void prologue(FuncOp *func);
  void relocateAlloca(FuncOp *func);
  void rewriteJumps(Block *bb);
  void cleanup();

  std::vector<Op*> clean;
) {
  prologue(func);
  relocateAlloca(func);

  auto region = func->getRegion();
  for (auto bb : *region)
    rewriteJumps(bb);

  cleanup();
}

void LateLegalize::prologue(FuncOp *func) {
  std::set<Reg> values;
  // Find out all caller-saved registers.
  walk(func, [&](Op *op) {
    for (auto ret : op->getResults())
      values.insert(assignment[ret]);
    if (isa<BlOp>(op))
      values.insert(x30);
  });
  std::vector<Reg> ints, floats;
  for (auto x : values) {
    if (!calleeSaved.contains(x))
      continue;
    (regbank(x) == FP ? floats : ints).push_back(x);
  }

  // Generate function prologue/epilogue.
  Builder builder;
  auto region = func->getRegion();
  builder.setToStart(region);
  for (unsigned i = 0; i < ints.size(); i += 2) {
    auto alloca = builder.create<AllocaOp>(Type::pointer(vi4))->ret();

    auto rd = createAssignedRd(builder, ints[i]);

    if (i + 1 < ints.size()) {
      auto rd2 = createAssignedRd(builder, ints[i + 1]);
      builder.create<StpOp>()->with(alloca, rd->ret(), rd2->ret());
    } else {
      builder.create<StrOp>()->with(alloca, rd->ret());
    }

    Builder::Guard guard(builder);
    builder.setBefore(region->getLastOp());
    
    if (i + 1 < ints.size()) {
      auto ldp = builder.create<LdpOp>(i64, i64)->with(alloca);
      
      assignment[ldp->ret(0)] = ints[i];
      assignment[ldp->ret(1)] = ints[i + 1];

      auto wr1 = builder.create<WriteRegOp>()->with(ldp->ret(0));
      wr1->reg = ints[i];

      auto wr2 = builder.create<WriteRegOp>()->with(ldp->ret(1));
      wr2->reg = ints[i + 1];
    } else {
      auto ldr = builder.create<LdrOp>(i64)->with(alloca);
      assignment[ldr->ret()] = ints[i];

      auto wr = builder.create<WriteRegOp>()->with(ldr->ret());
      wr->reg = ints[i];
    }
  }
}

void LateLegalize::relocateAlloca(FuncOp *func) {
  auto allocas = collectOps<AllocaOp>(func);
  auto region = func->getRegion();

  // We allocate shorter allocas first, so that more allocas can fit in immediate range of `ldr`.
  std::sort(allocas.begin(), allocas.end(), [](AllocaOp *l, AllocaOp *r) {
    return asmSize(l) < asmSize(r);
  });

  int total = 0;
  Builder builder;
  for (auto alloca : allocas) {
    builder.setToStart(region);
    int sz = asmSize(alloca);

    // This alloca is equivalently `sp + total`.
    auto rd = createAssignedRd(builder, sp);
    auto add = builder.create<AddXIOp>(i64)->with(rd->ret());
    add->value = total;
    // This should always have been combined.
    // If it isn't, then it's always `add sp, sp, #0` and is hence `sp`.
    assignment[add->ret()] = sp;
    clean.push_back(add);

    alloca->ret()->replaceAllUsesWith(add->ret());
    alloca->erase();

    total += sz;
  }

  // Finally, we must round up `total` to 16 bytes, and subtract `total` from sp.
  total = (total + 15) / 16 * 16;
  if (total > 0) {
    builder.setToStart(region);
    auto rd = createAssignedRd(builder, sp);

    auto add = builder.create<AddXIOp>(i64)->with(rd->ret());
    add->value = -total;
    assignment[add->ret()] = sp;

    auto wr = builder.create<WriteRegOp>()->with(add->ret());
    wr->reg = sp;

    // Similarly, it must be restored at the end of function.
    builder.setBefore(region->getLastOp());
    rd = createAssignedRd(builder, sp);

    add = builder.create<AddXIOp>(i64)->with(rd->ret());
    add->value = total;
    assignment[add->ret()] = sp;

    wr = builder.create<WriteRegOp>()->with(add->ret());
    wr->reg = sp;
  }

  // Try to fold the extra addition we've just introduced.
  fixed(walk<Postorder>(func, [&](Op *op) {
    for (auto &rule : rules) {
      if (rule.rewrite(op)) {
        mark_changed;
        return;
      }
    }

    // Also fold stp and ldp. They can't be captured by rules,
    // since `stp` would have operands >= 3 and `ldp` has 2 results.
    if (auto stp = dyn_cast<StpOp>(op)) {
      auto addr = dyn_cast<AddXIOp>(op->val()->def);
      if (!addr)
        return;

      op->setOperand(0, addr->val());
      stp->value += addr->value;
      return;
    }

    if (auto ldp = dyn_cast<LdpOp>(op)) {
      auto addr = dyn_cast<AddXIOp>(op->val()->def);
      if (!addr)
        return;

      op->setOperand(0, addr->val());
      ldp->value += addr->value;
      return;
    }
  });)
}

void LateLegalize::rewriteJumps(Block *bb) {
  auto op = bb->getLastOp();
  auto target = targetOf(op);
  auto other = elseOf(op);
  if (!target)
    return;

  // This is a jump. Omit it if it jumps to the next block.
  auto next = bb->nextBlock();
  if (!other) {
    if (target == next)
      op->erase();
    return;
  }

  // Now we know it's a branch.
  // When the `else` branch falls through, then it's unchanged.
  if (other == next) {
    setElse(op, nullptr);
    return;
  }

  // When the `target` branch falls through, the one must be flipped.
  if (target == next) {
    flip(op);
    return;
  }

  // Both branch don't fall through. Add a jump to `other`.
  Builder builder;
  builder.setAfter(op);
  setElse(op, nullptr);
  builder.create<BOp>()->target = other;
}

void LateLegalize::cleanup() {
  for (auto op : clean) {
    if (std::all_of(op->getResults().begin(), op->getResults().end(), [](Value *v){
      return !v->used();
    }))
      op->erase();
  }
  clean.clear();
}

}
