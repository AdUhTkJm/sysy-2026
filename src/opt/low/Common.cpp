#include "Common.h"

namespace opt {

int stackSlotSize(const Type *ty) {
  int z = asmSize(ty);
  if (z < 8)
    z = 8;
  return (z + 7) / 8 * 8;
}

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

int argLayout(const std::vector<const Type *> &types, std::vector<ArgLoc> &out) {
  out.clear();
  // Here x is the number of integer arguments passed in registers,
  //      v is the number of vector arguments passed in registers,
  //      total is the total number of bytes of stack slots needed.
  int x = 0, v = 0, total = 0;
  for (const Type *ty : types) {
    ArgLoc loc {};
    if (ir::regbank(ty) == INT) {
      if (x < 8) {
        loc.inReg = true;
        loc.reg = argRegs[x++];
      } else {
        int sz = stackSlotSize(ty);
        total = (total + sz - 1) / sz * sz;
        loc.inReg = false;
        loc.reg = xzr;
        loc.stackOffset = total;
        total += sz;
      }
    } else {
      if (v < 8) {
        loc.inReg = true;
        loc.reg = fargRegs[v++];
      } else {
        int sz = stackSlotSize(ty);
        total = (total + sz - 1) / sz * sz;
        loc.inReg = false;
        loc.reg = xzr;
        loc.stackOffset = total;
        total += sz;
      }
    }
    out.push_back(loc);
  }
  return (total + 15) / 16 * 16;
}

}
