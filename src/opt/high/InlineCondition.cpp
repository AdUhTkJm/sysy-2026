#include "Common.h"

namespace opt {

int asmSize(const Type *ty);

declare_pass(InlineCondition,
  void runImpl(DoWhileOp *loop) const;
) {
  for_all(DoWhileOp)
    runImpl(op);
}

// Remove the loop variable that is only used for loop condition,
// and substitute it with another one.
void InlineCondition::runImpl(DoWhileOp *loop) const {
  auto last = condition_of(loop);
  auto cond = last->val(0)->def;

  if (!isa<LtOp>(cond) || !isa<AddIOp>(cond->val(0)->def))
    return;

  Value *lim;
  // Find the induction variable.
  // This value must be used exactly once (in i + 'a).
  auto indvar = this->indvar(loop, &lim);
  if (!indvar || indvar->getUses().size() != 1)
    return;

  auto incr = increment(indvar);
  if (!incr)
    return;

  // We must also attempt to find another value that increases by a constant.
  Value *candidate = nullptr, *inc, *cstart;
  auto variants = getVariantsIn(loop);
  if (variants.count(lim))
    return;
  moveChainBefore(lim->def, loop, loop);

  for (unsigned i = 0; i < loop->getNumOperands(); i++) {
    // Don't change into the eliminated variable again.
    if (loop->ret(i) == indvar)
      continue;

    auto next = last->val(i + 1)->def;
    if ((!isa<AddIOp>(next) && !isa<AddLOp>(next)) || next->val(0) != loop->ret(i))
      continue;

    inc = next->val(1);
    if (variants.count(inc))
      continue;

    moveChainBefore(inc->def, loop, loop);
    candidate = loop->ret(i);
    cstart = loop->val(i);
    break;
  }
  if (!candidate)
    return;
  
  // The iteration count is ceilDiv(lim - start, incr).
  // It's always positive (the loop is always guarded by an if-statement).
  // So `lim - start - 1` will never underflow. We can rewrite it into
  //    (lim - start - 1) / incr + 1

  Builder builder;
  builder.setBefore(loop);
  auto start = loop->val(loop->getResultIndex(indvar));
  auto sub1 = builder.create<SubIOp>(i32)->with(lim, start);
  auto one = builder.createInt(1);
  auto sub2 = builder.create<SubIOp>(i32)->with(sub1->ret(), one->ret());
  auto div = builder.create<DivIOp>(i32)->with(sub2->ret(), incr);
  Op *iterations = builder.create<AddIOp>(i32)->with(div->ret(), one->ret());

  // Now we synthesize the new limit:
  //    cstart + iterations * inc
  auto type = i32;
  if (asmSize(inc->type) == 8) {
    iterations = builder.create<CastOp>(i64)->with(iterations->ret());
    type = i64;
  }
  
  Op *mul = type == i32
    ? (Op*) builder.create<MulIOp>(type)->with(iterations->ret(), inc)
    : (Op*) builder.create<MulLOp>(type)->with(iterations->ret(), inc);
  if (asmSize(cstart->type) == 8 && type != i64) {
    mul = builder.create<CastOp>(i64)->with(mul->ret());
    type = i64;
  }

  auto end = type == i32
    ? (Op*) builder.create<AddIOp>(type)->with(cstart, mul->ret())
    : (Op*) builder.create<AddLOp>(type)->with(cstart, mul->ret());

  // Then synthesize the loop condition: `candidate + inc < end`.
  builder.setBefore(last);
  auto add1 = type == i32
    ? (Op*) builder.create<AddIOp>(type)->with(candidate, inc)
    : (Op*) builder.create<AddLOp>(type)->with(candidate, inc);
  auto cond2 = builder.create<LtOp>(i32)->with(add1->ret(), end->ret());
  last->setOperand(0, cond2->ret());
}

}