#include "../Pass.h"           // IWYU pragma: keep
#include "../../ir/Builder.h"  // IWYU pragma: keep
#include "../../ir/Attrs.h"    // IWYU pragma: keep
#include "../../ir/Printer.h"  // IWYU pragma: keep
#include "../../ir/Regs.h"     // IWYU pragma: keep

using namespace ir;

namespace opt {

int asmSize(const Op *op);
int asmSize(const Type *ty);
int asmSize(const AllocaOp *op);
int stackSlotSize(const Type *ty);

ReadRegOp *createAssignedRd(Builder &builder, Reg reg, const Type *ty = i64);

struct ArgLoc {
  Reg reg;
  int stackOffset;
  bool inReg;
};

// Given a list of argument types, returns a list of argument locations
// and the total number of outgoing stack bytes (16-byte aligned).
int argLayout(const std::vector<const Type *> &types, std::vector<ArgLoc> &out);

// Registers that are not allowed for a specific value.
extern std::unordered_map<Value*, std::set<Reg>> bads;

}
