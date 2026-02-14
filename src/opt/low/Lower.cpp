#include "Common.h"

namespace opt {

#define rename(From, To) \
  for_all(From, func) \
    builder.rename<To>(op);

declare_pass(Lower,
  void runImpl(FuncOp *func);
  const DimAttr *getDim(Value *addr) const;

  bool hasInit = false;
  FuncOp *main = nullptr;
) {
  printer.setBlockPrefix(".L");
  for (auto x : collectFunctions())
    runImpl(x);

  if (hasInit) {
    assert(main && "main function must exist");
    Builder builder;
    builder.setToStart(main->getRegion());
    auto call = builder.create<BlOp>(unit);
    call->name = "__init";
  }
}

const DimAttr *Lower::getDim(Value *addr) const {
  auto base = baseOf(addr);
  if (auto dim = base->get<DimAttr>())
    return dim;

  // This has to be a function argument.
  assert(isa<FuncOp>(base));
  auto index = base->getOperandIndex(addr);
  for (auto call : base->ret()->getUses())
    return getDim(call->val(index + 1));

  assert(false && "the function has to be used!");
}

void Lower::runImpl(FuncOp *func) {
  Builder builder;
  func->getRegion()->convertToPhi();

  rename(AddIOp, AddWOp);
  rename(AddLOp, AddXOp);
  rename(SubIOp, SubWOp);
  rename(MulIOp, MulWOp);
  rename(DivIOp, DivWOp);
  rename(EqOp, CmpEqOp);
  rename(NeOp, CmpNeOp);
  rename(LtOp, CmpLtOp);
  rename(LeOp, CmpLeOp);
  
  for_all(NotOp, func) {
    builder.setBefore(op);
    auto movi = builder.create<MovIOp>(op->ret()->type);
    movi->value = 0;

    auto val = op->val();
    builder.replace<CmpEqOp>(op, i32)->with(val, movi->ret());
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
  for_all(ArrayStoreOp, func) {
    builder.setBefore(op);

    Value *dest = op->val(0), *val = op->val(op->getNumOperands() - 1);
    auto dim = getDim(dest);
    assert(dim);
    const auto &dims = dim->dims;
    
    int stride = asmSize(val->type);
    for (int i = (int) dims.size() - 1; i >= 0; i--) {
      auto movi = builder.create<MovIOp>(i32);
      movi->value = stride;

      auto mul = builder.create<MulWOp>(i32)->with(op->val(i + 1), movi->ret());
      // TODO: This is a type mismatch. Maybe add zext here?
      dest = builder.create<AddXOp>(i64)->with(dest, mul->ret())->ret();
      stride *= dims[i];
    }

    builder.replace<StrOp>(op)->with(dest, val);
  }
  for_all(ArrayLoadOp, func) {
    builder.setBefore(op);

    Value *dest = op->val(0);
    auto dim = getDim(dest);
    assert(dim);
    const auto &dims = dim->dims;
    
    int stride = asmSize(op->ret()->type);
    // Same as the one before.
    for (int i = (int) dims.size() - 1; i >= 0; i--) {
      auto movi = builder.create<MovIOp>(i32);
      movi->value = stride;

      auto mul = builder.create<MulWOp>(i32)->with(op->val(i + 1), movi->ret());
      dest = builder.create<AddXOp>(i64)->with(dest, mul->ret())->ret();
      stride *= dims[i];
    }

    auto ty = op->ret()->type;
    builder.replace<LdrOp>(op, ty)->with(dest);
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

  // Lower function arguments as reads to physical registers.
  builder.setToStart(func->getRegion());
  for (auto [i, arg] : data::enumerate(func->getResults())) {
    if (i == 0)
      continue;
    // TODO: what happens when i >= 8?
    auto rd = builder.create<ReadRegOp>(arg->type);
    rd->reg = regbank(arg->type) == INT ? argRegs[i - 1] : fargRegs[i - 1];
    arg->replaceAllUsesWith(rd->ret());
  }
  for (unsigned i = func->getNumResults() - 1; i > 0; i--)
    func->removeResult(i);

  if (func->name == "__init")
    hasInit = true;
  if (func->name == "main")
    main = func;
};

}
