#include "Common.h" // IWYU pragma: keep

namespace opt {

std::unordered_map<Value*, std::set<Reg>> bads;

#define rename(From, To) \
  for_all(From, func) \
    builder.rename<To>(op);

declare_pass(Lower,
  void runImpl(FuncOp *func);
) {
  printer.setBlockPrefix(".L");
  for (auto x : collectFunctions())
    runImpl(x);

  for_all(PhiOp) {
    if (op->getNumOperands() != 1)
      continue;

    op->ret()->replaceAllUsesWith(op->val());
    op->erase();
  }
}

void Lower::runImpl(FuncOp *func) {
  Builder builder;
  func->getRegion()->convertToPhi();

  rename(AddIOp, AddWOp);
  rename(AddLOp, AddXOp);
  rename(SubIOp, SubWOp);
  rename(SubLOp, SubXOp);
  rename(MulIOp, MulWOp);
  rename(DivIOp, DivWOp);
  rename(AddFOp, FaddOp);
  rename(SubFOp, FsubOp);
  rename(MulFOp, FmulOp);
  rename(DivFOp, FdivOp);
  rename(EqOp, CmpEqOp);
  rename(NeOp, CmpNeOp);
  rename(LtOp, CmpLtOp);
  rename(LeOp, CmpLeOp);
  rename(EqFOp, FcmpEqOp);
  rename(NeFOp, FcmpNeOp);
  rename(LtFOp, FcmpLtOp);
  rename(LeFOp, FcmpLeOp);
  rename(F2IOp, FcvtzsOp);
  rename(I2FOp, ScvtfOp);
  rename(AndIOp, AndWOp);
  rename(SextOp, SxtwOp);
  rename(LShiftOp, LslWOp);
  rename(RShiftOp, LsrWOp);
  
  for_all(NotOp, func) {
    builder.setBefore(op);
    auto movi = builder.create<MovIOp>(op->ret()->type);
    movi->value = 0;

    auto val = op->val();
    builder.replace<CmpEqOp>(op, i32)->with(val, movi->ret());
  }
  for_all(NotFOp, func) {
    builder.setBefore(op);
    // This will be translated later in the function.
    auto zero = builder.createFloat(0);

    auto val = op->val();
    builder.replace<FcmpEqOp>(op, i32)->with(val, zero->ret());
  }
  for_all(ModIOp, func) {
    // a % b => a - (a / b) * b
    //
    // sdiv x2, x0, x1
    // msub x3, x2, x1, x0 (This is x0 - x1 * x2)
    builder.setBefore(op);
    auto x0 = op->val(0), x1 = op->val(1);
    auto x2 = builder.create<DivWOp>(i32)->with(x0, x1)->ret();
    builder.replace<MsubWOp>(op, i32)->with(x2, x1, x0);
  }
  for_all(IntOp, func) {
    auto i = op->value;
    auto li = builder.rename<MovIOp>(op);
    li->value = i;
  }
  for_all(Int64Op, func) {
    auto i = op->value;
    auto li = builder.rename<MovLOp>(op);
    // We don't want precision loss.
    // In fact we should have this because `int64op` is only generated from sext + int;
    // But assert here for safety.
    assert(i < (1ll << 32) && i >= -(1ll << 32));
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
    bl->name = std::move(name);
  }
  for_all(JumpOp, func) {
    auto target = op->target;
    auto b = builder.rename<BOp>(op);
    b->target = target;
  }
  for_all(BranchOp, func) {
    auto target = op->target, other = op->other;
    auto cbz = builder.rename<CbnzOp>(op);
    cbz->target = target;
    cbz->other = other;
  }
  for_all(LoadOp, func) {
    auto ld = builder.rename<LdrOp>(op);
    ld->value = 0;
  }
  for_all(StoreOp, func) {
    auto ld = builder.rename<StrOp>(op);
    ld->value = 0;
  }
  for_all(FloatOp, func) {
    int v = *(int *) &op->value;
    builder.setBefore(op);
    auto li = builder.create<MovIOp>(i32);
    li->value = v;
    builder.replace<FmovOp>(op, f32)->with(li->ret());
  }

  // Lower GetGlobals after we finished accessing arrays.
  for_all(GetGlobalOp, func) {
    auto global = cast<GlobalOp>(op->val(0)->def);
    auto name = global->name;
    builder.setBefore(op);
    auto ty = op->ret()->type;
    auto la = builder.create<AdrpOp>(ty);
    la->name = name;
    auto addx = builder.replace<AddXPOp>(op, ty)->with(la->ret());
    addx->name = std::move(name);
  }

  // Lower function arguments: registers or incoming stack slots (see LateLegalize).
  builder.setToStart(func->getRegion());
  std::vector<const Type*> ptypes;
  for (auto [i, arg] : data::enumerate(func->getResults())) {
    if (i == 0)
      continue;
    ptypes.push_back(arg->type);
  }
  std::vector<ArgLoc> locs;
  argLayout(ptypes, locs);
  auto spRd = createAssignedRd(builder, sp);
  unsigned j = 0;

  std::vector<ReadRegOp*> ops;
  for (auto [i, arg] : data::enumerate(func->getResults())) {
    if (i == 0)
      continue;
    const auto &loc = locs[j++];
    if (loc.inReg) {
      auto rd = builder.create<ReadRegOp>(arg->type);
      rd->reg = loc.reg;
      arg->replaceAllUsesWith(rd->ret());

      // Note that each ReadRegOp collides with later ones.
      for (auto x : ops)
        bads[x->ret()].insert(loc.reg);

      ops.push_back(rd);
    } else {
      auto ldr = builder.create<LdrOp>(arg->type)->with(spRd->ret());
      ldr->value = loc.stackOffset;
      ldr->set<IncomingStackArgAttr>();
      arg->replaceAllUsesWith(ldr->ret());
    }
  }
  for (unsigned i = func->getNumResults() - 1; i > 0; i--)
    func->removeResult(i);
};

}
