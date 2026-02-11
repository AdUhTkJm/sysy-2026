#include "Ops.h"
#include "../utils/DataStructure.h"
#include <algorithm>

namespace ir {

Value *FuncOp::getHandle() const {
  return ret(0);
}

std::vector<Value*> FuncOp::getArgs() const {
  return std::vector(results.begin() + 1, results.end());
}

Value *FuncOp::getArg(unsigned i) const {
  return ret(i + 1);
}

void PhiOp::addIncoming(Value *v, Block *bb) {
  pushOperand(v);
  targets.push_back(bb);
}

Value *PhiOp::incomingFrom(const Block *bb) const {
  for (auto [i, t] : data::enumerate(targets)) {
    if (t == bb)
      return val(i);
  }
  assert(false && "no incoming from `bb`!");
  return nullptr;
}

void PhiOp::removeIncoming(Block *bb) {
  auto it = std::find(targets.begin(), targets.end(), bb);
  if (it == targets.end())
    return;

  size_t index = it - targets.begin();
  targets.erase(it);
  removeOperand(index);
}

void PhiOp::replaceIncoming(Block *bb, Block *after) {
  auto it = std::find(targets.begin(), targets.end(), bb);
  if (it == targets.end())
    return;

  *it = after;
}

#define isa(Ty) isa<Ty>(op) ||
bool hasSideEffect(Op *op) {
  return (impure_op_list(isa) false);
}
#undef isa

// Takes `AddIOp` to `addi`.
std::string stripped(const char *s) {
  std::string v;
  for (auto p = s; *p; p++) {
    v.push_back(tolower(*p));
  }
  v.pop_back();
  v.pop_back();
  return v;
}

#define opkind_name_map(Ty) \
  { stripped(#Ty), OpKind::Ty },
#define opkind_name_rev(Ty) \
  case OpKind::Ty: return #Ty;

const std::map<std::string, OpKind> opkindNames {
  complete_op_list(opkind_name_map)
};

const char *kindname(OpKind kind) {
  switch (kind) {
  complete_op_list(opkind_name_rev)
  default:
    assert(false && "not a valid opkind!");
  }
}

}
