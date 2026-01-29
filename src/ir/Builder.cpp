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

}
