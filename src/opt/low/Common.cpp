#include "Common.h"

namespace opt {

int asmSize(const Type *ty) {
  if (ty == i32 || ty == f32)
    return 4;

  if (ty == i64 || ty->kind == Type::ptr || ty->kind == Type::fn)
    return 8;

  if (ty == vf4 || ty->vi4)
    return 16;

  if (ty == unit)
    return 0;

  assert(false && "unexpected type");
}

int asmSize(const Op *op) {
  int base = asmSize(op->ret()->type);
  if (auto dim = op->get<DimAttr>())
    return dim->size() * base;

  return base;
}

ReadRegOp *createAssignedRd(Builder &builder, Reg reg, const Type *ty) {
  auto rd = builder.create<ReadRegOp>(ty);
  assignment[rd->ret()] = reg;
  rd->reg = reg;
  return rd;
}

}
