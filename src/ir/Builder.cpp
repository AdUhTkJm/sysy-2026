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
  op->results.push_back(new Value(i32, op, 0));
  op->value = i;
  insert(op);
  return op;
}

FloatOp *Builder::createFloat(float f) {
  auto op = new FloatOp(bb, at);
  op->results.push_back(new Value(f32, op, 0));
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
  caseof(Ty) basecopy(Ty); break;
#define clone_targetful(Ty) \
  caseof(Ty) { \
    auto target = cast<Ty>(op)->target; \
    auto x = basecopy(Ty); \
    x->target = target; \
    break; \
  }

Op *Builder::clone(Op *op) {
  switch (op->id) {
  empty_op_list(clone_empty)
  targetful_op_list(clone_targetful)
  }

  std::cout << "unknown op kind: " << kindname((OpKind) op->id);
  assert(false && "cannot get here!");
  return nullptr;
}

void Builder::copy(Block *bb, Map &map) {
  for (auto op : *bb) {
    auto cloned = clone(op);
    for (auto [i, ret] : data::enumerate(cloned->getResults()))
      map[op->ret(i)] = ret;
  }
  // Rewire operands.
  for (auto op : *bb) {
    for (unsigned i = 0; i < op->getNumOperands(); i++) {
      auto it = map.find(op->val(i));
      if (it == map.end())
        continue;

      op->setOperand(i, it->second);
    }
  }
}

}
