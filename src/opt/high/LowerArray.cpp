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

  std::unordered_map<ArrayLoadOp*, std::vector<int>> dimmap;

  // It is possible that ArrayLoadOp only dereferences a certain amount of dimensions,
  // but not all.
  // We must collect the dimensions beforehand, since lowered ArrayLoads lose this
  // information.
  for_all(ArrayLoadOp, func)
    dimmap[op] = getDim(op->val());
  
  for_all(ArrayLoadOp, func) {
    builder.setBefore(op);

    const auto &dims = dimmap[op];
    auto dest = op->val();

    int stride = asmSize(op->ret()->type);
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

int derefedDims(Value *addr) {
  auto op = addr->def;
  if (isa<ArrayLoadOp>(op))
    return derefedDims(op->val()) + op->getNumOperands() - 1;

  return 0;
}

std::vector<int> LowerArray::getDim(Value *addr) const {
  auto base = baseOf(addr);
  int derefed = derefedDims(addr);
  std::vector<int> result;
  const std::vector<int> *dims;
  if (auto dim = base->get<DimAttr>())
    dims = &dim->dims;
  else {
    assert(isa<FuncOp>(base));
    dims = &base->get<ArgDimAttr>()->dims.at(addr);
  }
    
  result.reserve(dims->size() - derefed);
  for (unsigned i = derefed; i < dims->size(); i++)
    result.push_back((*dims)[i]);
  return result;
}

}
