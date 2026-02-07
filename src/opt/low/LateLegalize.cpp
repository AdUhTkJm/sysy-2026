#include "Common.h"
#include <algorithm>

namespace opt {

static int asmSize(AllocaOp *op) {
  auto sz = asmSize(op->ret()->type->pointee());
  if (auto dim = op->get<DimAttr>())
    sz *= dim->size();
  return sz;
}

ReadRegOp *createAssignedRd(Builder &builder, Reg reg) {
  auto rd = builder.create<ReadRegOp>(i64);
  assignment[rd->ret()] = reg;
  rd->reg = reg;
  return rd;
}

declare_local_pass(LateLegalize,
  void prologue(FuncOp *func);
  void relocateAlloca(FuncOp *func);
) {
  prologue(func);
  relocateAlloca(func);
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
  builder.setToStart(region);
  for (auto alloca : allocas) {
    int sz = asmSize(alloca);
    total += sz;

    // This alloca is equivalently `sp + sz`.
    auto rd = createAssignedRd(builder, sp);

    auto add = builder.create<AddXIOp>(i64)->with(rd->ret());
    add->value = sz;
    // This should always have been combined.
    assignment[add->ret()] = unallocated;

    alloca->ret()->replaceAllUsesWith(add->ret());
    alloca->erase();
  }

  // Finally, we must round up `total` to 16 bytes, and subtract `total` from sp.
  total = (total + 15) / 16 * 16;
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

}
