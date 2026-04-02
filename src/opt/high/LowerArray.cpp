#include "../low/Common.h"

namespace opt {

declare_local_pass(LowerArray,
  std::vector<int> getDim(Value *addr) const;
) {
  Builder builder;
  for_all(ArrayStoreOp, func) {
    builder.setBefore(op);

    Value *dest = op->val(0), *val = op->val(op->getNumOperands() - 1);
    auto dims = getDim(dest);
    
    int stride = asmSize(val->type);
    for (int i = (int) dims.size() - 1; i >= 0; i--) {
      auto movi = builder.createInt(stride);

      auto mul = builder.create<MulIOp>(i32)->with(op->val(i + 1), movi->ret());
      auto sext = builder.create<SextOp>(i64)->with(mul->ret());
      dest = builder.create<AddLOp>(i64)->with(dest, sext->ret())->ret();
      stride *= dims[i];
    }

    builder.replace<StoreOp>(op)->with(dest, val);
  }
  for_all(ArrayLoadOp, func) {
    builder.setBefore(op);

    Value *dest = op->val(0);
    auto dims = getDim(dest);
    
    int stride = asmSize(op->ret()->type);
    // It is possible that ArrayLoadOp only dereferences a certain amount of dimensions,
    // but not all.
    int i = dims.size() - 1;
    while (i + 1 >= (int) op->getNumOperands())
      stride *= dims[i--];

    bool fulldim = i + 1 == (int) dims.size();
    for (; i >= 0; i--) {
      auto movi = builder.createInt(stride);

      auto mul = builder.create<MulIOp>(i32)->with(op->val(i + 1), movi->ret());
      auto sext = builder.create<SextOp>(i64)->with(mul->ret());
      dest = builder.create<AddLOp>(i64)->with(dest, sext->ret())->ret();
      stride *= dims[i];
    }

    if (fulldim) {
      auto ty = op->ret()->type;
      builder.replace<LoadOp>(op, ty)->with(dest);
    } else {
      op->ret()->replaceAllUsesWith(dest);
      op->erase();
    }
  }
}

std::vector<int> LowerArray::getDim(Value *addr) const {
  auto base = baseOf(addr);
  std::cerr << addr->def << "\n";
  if (auto dim = base->get<DimAttr>())
    return dim->dims;

  // This has to be a function argument. TODO: no function argument can be an array.
  assert(isa<FuncOp>(base));
  return base->get<ArgDimAttr>()->dims.at(addr);
}

}
