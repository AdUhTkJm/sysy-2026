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
  func->getRegion()->convertToPhi();
};

}
