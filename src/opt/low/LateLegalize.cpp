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

#define every(f) \
  for (auto bb : *region) \
    for (auto it = bb->begin(); it != bb->end();) { \
      auto next = it; ++next; \
      f(*it); \
      it = next; \
    }

static Op *flip(Op *op) {
  Builder builder;
  cvt(CbzOp, CbnzOp)
  cvt(BeqOp, BneOp)
  cvt(BltOp, BgeOp)
  cvt(BleOp, BgtOp)
  assert(false && "should only flip branches!");
}

declare_local_pass(LateLegalize,
  // The `prologue()` function expects a single return instruction at end,
  // to insert epilogue.
  void ensureSingleReturn(FuncOp *func);
  void prologue(FuncOp *func);
  void relocateAlloca(FuncOp *func);
  void rewriteJumps(Block *bb);
  void ensureImmRange(Op *op);
) {
  ensureSingleReturn(func);
  prologue(func);
  relocateAlloca(func);

  auto region = func->getRegion();
  for (auto bb : *region)
    rewriteJumps(bb);

  every(ensureImmRange)
}

void LateLegalize::ensureSingleReturn(FuncOp *func) {
  std::vector<Block*> returning;
  auto region = func->getRegion();

  for (auto bb : *region) {
    if (isa<RetOp>(bb->getLastOp()))
      returning.push_back(bb);
  }
  assert(!returning.empty());

  if (returning.size() == 1) {
    auto bb = returning[0];
    bb->moveToEnd(region);
    return;
  }

  Builder builder;
  auto end = region->appendBlock();

  builder.setToEnd(end);
  builder.create<RetOp>();

  for (auto bb : returning) {
    auto b = builder.replace<BOp>(bb->getLastOp());
    b->target = end;
  }
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
    if (!calleeSaved.count(x))
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

#define check_alloca \
  auto base = baseOf(op->val()); \
  if (!base) \
    return; \
  auto alloca = dyn_cast<AllocaOp>(base); \
  if (!alloca) \
    return; \

#define relocate_fold(Ty) \
  if (auto x = dyn_cast<Ty>(op)) { \
    check_alloca \
    x->value += offsets[alloca]; \
  }

void LateLegalize::relocateAlloca(FuncOp *func) {
  auto allocas = collectOps<AllocaOp>(func);
  auto region = func->getRegion();

  // We allocate shorter allocas first, so that more allocas can fit in immediate range of `ldr`.
  std::unordered_map<AllocaOp*, int> priority;
  for (auto x : allocas)
    priority[x] = -asmSize(x);

  // We always prioritze ldps and stps, so that they fall in the range of [-512, 504].
  for_all(LdpOp, func)
    priority[cast<AllocaOp>(op->val(0)->def)] += 0x3000'0000;
  for_all(StpOp, func)
    priority[cast<AllocaOp>(op->val(0)->def)] += 0x3000'0000;

  std::sort(allocas.begin(), allocas.end(), [&](AllocaOp *l, AllocaOp *r) {
    return priority[l] > priority[r];
  });

  int total = 0;
  if (auto off = func->get<StackOffsetAttr>())
    total = off->size;
  std::map<AllocaOp*, int> offsets;
  std::vector<Op*> clean;

  Builder builder;
  builder.setToStart(region);
  auto rd = createAssignedRd(builder, sp);
  for (auto alloca : allocas) {
    int sz = asmSize(alloca);

    // This alloca is equivalently `sp + total`.
    // But we have to round it up to a multiple of `sz`.
    offsets[alloca] = (total + sz - 1) / sz * sz;
    clean.push_back(alloca);

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

  for_all(LdrOp, func) {
    if (op->has<IncomingStackArgAttr>())
      op->value += total;
  }

  // Fold the extra addition we've just introduced.
  // An alloca can only be used for loads, stores or passed as argument to some other function.
  // The final possibility is already lowered to `WriteRegOp`, so we must also check that.
  fixed(walk<Postorder>(func, [&](Op *op) {
    arm_mem_op_list(relocate_fold)

    if (auto x = dyn_cast<WriteRegOp>(op)) {
      check_alloca
      builder.setBefore(op);

      auto add = builder.create<AddXIOp>(i64)->with(rd->ret());
      add->value = offsets[alloca];
      assignment[add->ret()] = (Reg) x->reg;

      x->erase();
      return;
    }
  });)

  for (auto alloca : clean) {
    alloca->ret()->replaceAllUsesWith(rd->ret());
    alloca->erase();
  }
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

#define imm_range_list(X) \
  X(LdrOp, -16384, 16380) X(StrOp, -16384, 16380)

// Warning: this is actually erratic.
#define rewrite_imm_range(Ty, l, r) \
  if (auto x = dyn_cast<Ty>(op)) { \
    if (x->value <= r && x->value >= l) \
      return; \
    builder.setBefore(op); \
    auto val = x->val(0); \
    auto addx = builder.create<AddXIOp>(val->type)->with(val); \
    assignment[addx->ret()] = scratch[0]; \
    addx->value = x->value; \
    x->value = 0; \
    x->setOperand(0, addx->ret()); \
    return; \
  }

void LateLegalize::ensureImmRange(Op *op) {
  Builder builder;
  imm_range_list(rewrite_imm_range);

  if (auto movi = dyn_cast<MovIOp>(op); movi && (unsigned) movi->value > 0xffff) {
    builder.setAfter(movi);
    auto movk = builder.create<MovKOp>(i32);
    movk->value = (unsigned) movi->value >> 16;
    movi->value &= 0xffff;
    assignment[movk->ret()] = assignment[movi->ret()];
    return;
  }

  if (auto movl = dyn_cast<MovLOp>(op)) {
    int val = movl->value;

    builder.setAfter(movl);
    Value *dst = movl->ret();
    Value *v;

    if (val >= 0) {
      unsigned short lo = val & 0xffff;
      unsigned short hi = (val >> 16) & 0xffff;

      auto base = builder.create<MovZOp>(i64);
      base->value = lo;
      assignment[base->ret()] = assignment[dst];
      v = base->ret();

      if (hi != 0) {
        auto movk = builder.create<MovKOp>(i64);
        movk->value = hi;
        assignment[movk->ret()] = assignment[dst];
        v = movk->ret();
      }
    } else {
      unsigned short lo = (~val) & 0xffff;
      unsigned short hi = (val >> 16) & 0xffff;

      auto base = builder.create<MovNOp>(i64);
      base->value = lo;
      assignment[base->ret()] = assignment[dst];
      v = base->ret();

      if (hi != 0xffff) {
        auto movk = builder.create<MovKOp>(i64);
        movk->value = hi;
        assignment[movk->ret()] = assignment[dst];
        v = movk->ret();
      }
    }
    movl->ret()->replaceAllUsesWith(v);
    movl->erase();
    return;
  }
}

}
