#include "Builder.h"
#include "Ops.h"
#include "Attrs.h"

namespace ir {

void Builder::insert(Op *op) {
  assert(bb && "builder uninitialized!");
  bb->insert(at, op);
}

void Builder::setAfter(Op *op) {
  bb = op->parent;
  at = op->place; at++;
}

void Builder::setBefore(Op *op) {
  bb = op->parent;
  at = op->place;
}

void Builder::setToStart(Block *block) {
  bb = block;
  at = block->begin();
}

void Builder::setToEnd(Block *block) {
  bb = block;
  at = block->end();
}

IntOp *Builder::createInt(int i) {
  auto op = new IntOp(bb, at);
  op->results.push_back(new Value(i32, op));
  op->value = i;
  insert(op);
  return op;
}

FloatOp *Builder::createFloat(float f) {
  auto op = new FloatOp(bb, at);
  op->results.push_back(new Value(f32, op));
  op->value = f;
  insert(op);
  return op;
}

ModuleOp *Builder::createModule() {
  return new ModuleOp(nullptr, OpList::iterator());
}

#define caseof(Ty) case (int) OpKind::Ty:
#define basecopy(Ty) create<Ty>(op->getResultTypes())->with(op->getOperands())
#define clone_empty(Ty) \
  caseof(Ty) { \
    auto x = basecopy(Ty); \
    return x; \
  }
#define clone_targetful(Ty) \
  caseof(Ty) { \
    auto target = cast<Ty>(op)->target; \
    auto x = basecopy(Ty); \
    x->target = target; \
    return x; \
  }
#define clone_branch(Ty) \
  caseof(Ty) { \
    auto target = cast<Ty>(op)->target; \
    auto other = cast<Ty>(op)->other; \
    auto x = basecopy(Ty); \
    x->target = target; \
    x->other = other; \
    return x; \
  }
#define clone_imm(Ty) \
  caseof(Ty) { \
    auto value = cast<Ty>(op)->value; \
    auto x = basecopy(Ty); \
    x->value = value; \
    return x; \
  }
#define clone_regful(Ty) \
  caseof(Ty) { \
    auto reg = cast<Ty>(op)->reg; \
    auto x = basecopy(Ty); \
    x->reg = reg; \
    return x; \
  }
#define clone_nameful(Ty) \
  caseof(Ty) { \
    auto name = cast<Ty>(op)->name; \
    auto x = basecopy(Ty); \
    x->name = name; \
    return x; \
  }

#define regful_op_list(X) X(ReadRegOp) X(WriteRegOp)

Op *Builder::cloneImpl(Op *op) {
  switch (op->id) {
  empty_op_list(clone_empty)
  targetful_op_list(clone_targetful)
  branch_op_list(clone_branch)
  imm_op_list(clone_imm)
  regful_op_list(clone_regful)
  nameful_op_list(clone_nameful)
  }

  std::cerr << "unknown op kind: " << kindname((OpKind) op->id) << "\n";
  assert(false && "cannot get here!");
  return nullptr;
}

Op *Builder::clone(Op *op, Map &map) {
  auto cloned = cloneImpl(op);
  cloned->attrs = op->attrs;

  for (auto [i, ret] : data::enumerate(cloned->getResults())) {
    map[op->ret(i)] = ret;
    // Copy register assignments, if present.
    if (auto it = assignment.find(ret); it != assignment.end())
      assignment[op->ret(i)] = it->second;
  }

  Builder::Guard _(*this);
  for (auto r : op->getRegions()) {
    auto region = cloned->appendRegion();
    region->remove(region->getFirstBlock());

    for (auto bb : *r) {
      auto newbb = region->appendBlock();
      setToStart(newbb);
      copy(bb, map);
    }
  }

  return cloned;
}

Op *Builder::clone(Op *op) {
  Map map;
  return clone(op, map);
}

void Builder::copy(Block *bb, Map &map) {
  std::vector<Op*> total;
  for (auto op : *bb) {
    auto cloned = clone(op, map);
    total.push_back(cloned);
  }

  // Rewire operands.
  for (auto op : total) {
    for (unsigned i = 0; i < op->getNumOperands(); i++) {
      auto it = map.find(op->val(i));
      if (it == map.end())
        continue;

      op->setOperand(i, it->second);
    }
  }
}

}
