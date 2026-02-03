#include "Common.h"

namespace opt {

#define rename(From, To) \
  for_all(From, func) \
    builder.rename<To>(op);

declare_pass(Lower,
  void runImpl(FuncOp *func);
) {
  printer.setBlockPrefix(".L");
  for (auto x : collectFunctions())
    runImpl(x);
}

void Lower::runImpl(FuncOp *func) {
  Builder builder;
  rename(AddIOp, AddWOp);
  rename(AddLOp, AddXOp);
  rename(SubIOp, SubWOp);
  rename(MulIOp, MulWOp);
  rename(DivIOp, DivWOp);
  rename(EqOp, CmpEqOp);
  rename(NeOp, CmpNeOp);
  rename(LtOp, CmpLtOp);
  rename(LeOp, CmpLeOp);
  
  for_all(IntOp, func) {
    auto i = op->value;
    auto li = builder.rename<MovIOp>(op);
    li->value = i;
  }
  for_all(CallOp, func) {
    // Retrieve and remove the function handle.
    auto fn = cast<FuncOp>(op->val(0)->def);
    op->removeOperand(0);
    auto bl = builder.rename<BlOp>(op);
    bl->name = fn->name;
  }
  for_all(ExternCallOp, func) {
    auto name = std::move(op->name);
    auto bl = builder.rename<BlOp>(op);
    bl->name = name;
  }
  for_all(JumpOp, func) {
    auto target = op->target;
    auto b = builder.rename<BOp>(op);
    b->target = target;
  }
  for_all(BranchOp, func) {
    auto target = op->target, other = op->other;
    auto cbz = builder.rename<CbzOp>(op);
    cbz->target = target;
    cbz->other = other;
  }

  // Lower function arguments as reads to physical registers.
  builder.setToStart(func->getRegion());
  for (auto [i, arg] : data::enumerate(func->getResults())) {
    if (i == 0)
      continue;
    auto rd = builder.create<ReadRegOp>(arg->type);
    rd->reg = regbank(arg->type) == INT ? argRegs[i] : fargRegs[i];
    arg->replaceAllUsesWith(rd->ret());
  }
  for (unsigned i = func->getNumResults() - 1; i > 0; i--)
    func->removeResult(i);

  func->getRegion()->convertToPhi();
};

}
