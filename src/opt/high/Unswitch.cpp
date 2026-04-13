#include "Common.h"

namespace opt {

declare_pass(Unswitch,
  bool runImpl(DoWhileOp *loop);
  bool handleLt(IfOp *br, DoWhileOp *loop);

  Value *loopvar, *incr, *limit, *loopvarStart;
  std::set<Value*> variants;
  std::set<Op*> dead;
) {
  fixed(for_all(DoWhileOp)
    if (runImpl(op)) {
      mark_changed;
      break;
    }
  );
}

bool Unswitch::runImpl(DoWhileOp *loop) {
  // First recognize `if` statements related to incrementing loop variables.
  variants = getVariantsIn(loop);
  loopvar = indvar(loop, &limit);
  if (!loopvar)
    return false;

  incr = increment(loopvar);
  if (!incr)
    return false;
  loopvarStart = loop->val(loop->getResultIndex(loopvar));

  bool changed = false;
  fixed(for_all(IfOp, loop) {
    if (handleLt(op, loop)) {
      mark_changed;
      changed = true;
      break;
    }
  })
  return changed;
}

bool Unswitch::handleLt(IfOp *br, DoWhileOp *loop) {
  // We expect `loopvar < invariant`.
  auto cond = br->val()->def;
  if (!isa<LtOp>(cond) || variants.count(cond->val(1)) || cond->val(0) != loopvar)
    return false;

  // Now we can split the if-operation into two parts.
  // Both do-whiles are not always guaranteed to be executed;
  // We need to insert ifs around them.

  // Put the loop into a `if`.
  Builder builder;
  builder.setAfter(loop);
  auto if1 = builder.create<IfOp>();
  auto ifso1 = if1->appendRegion();
  auto ifnot1 = if1->appendRegion();
  loop->moveToStart(ifso1->getFirstBlock());
  
  builder.setToEnd(ifso1);
  auto soyield1 = builder.create<YieldOp>();
  builder.setToEnd(ifnot1);
  auto notyield1 = builder.create<YieldOp>();
  
  // Copy the loop inside another `if`.
  builder.setAfter(if1);
  auto if2 = builder.create<IfOp>();
  auto ifso2 = if2->appendRegion();
  auto ifnot2 = if2->appendRegion();

  builder.setToEnd(ifso2);
  Builder::OpMap mapping;
  auto later = builder.clone(loop, mapping);
  auto soyield2 = builder.create<YieldOp>();

  builder.setToEnd(ifnot2);
  auto notyield2 = builder.create<YieldOp>();

  for (unsigned i = 0; i < later->getNumOperands(); i++) {
    // The yields need to refer to corresponding values.
    soyield1->pushOperand(loop->ret(i));
    notyield1->pushOperand(loop->val(i));
    if1->pushResult(loop->ret(i)->type);

    soyield2->pushOperand(later->ret(i));
    notyield2->pushOperand(if1->ret(i));
    if2->pushResult(loop->ret(i)->type);

    // The second loop's starting values are ending values of the previous loop.
    // This is returned by the first loop's `if`.
    later->setOperand(i, if1->ret(i));

    // All later uses of the values should be converted to referring to the second `if`.
    loop->ret(i)->replaceAllUsesThat(if2->ret(i), [=](Op *op) {
      return !op->inside(if1) && !op->inside(if2);
    });
  }

  // Set the if condition: the loop is executed at least once.
  // i.e. `start < changepoint` for first, `start < limit` for second.
  auto changepoint = cond->val(1);
  if (changepoint->def->inside(loop))
    changepoint->def->moveBefore(if1);
  builder.setBefore(if1);
  auto ltcond = builder.create<LtOp>(i32)->with(loopvarStart, changepoint);
  if1->pushOperand(ltcond->ret());

  builder.setBefore(if2);
  ltcond = builder.create<LtOp>(i32)->with(if1->ret(loop->getResultIndex(loopvar)), limit);
  if2->pushOperand(ltcond->ret());
 
  // Now the first loop is where the condition is satisfied.
  // So we change the condition to `loopvar + incr < invariant`.
  auto last = condition_of(loop);
  builder.setBefore(last);
  auto add = builder.create<AddIOp>(i32)->with(loopvar, incr);
  auto end = builder.create<MinOp>(i32)->with(changepoint, limit);
  auto newcond = builder.create<LtOp>(i32)->with(add->ret(), end->ret());
  last->setOperand(0, newcond->ret());

  // Rewrite the results of the if.
  auto cr = mapping[br];
  assert(cr);
  auto yield = br->getRegion(0)->getLastOp();
  if (isa<YieldOp>(yield)) {
    for (unsigned i = 0; i < yield->getNumOperands(); i++)
      br->ret(i)->replaceAllUsesWith(yield->val(i));
    yield->erase();
  }
  yield = cr->getRegion(1)->getLastOp();
  if (isa<YieldOp>(yield)) {
    for (unsigned i = 0; i < yield->getNumOperands(); i++)
      cr->ret(i)->replaceAllUsesWith(yield->val(i));
    yield->erase();
  }

  // In the first loop, the `if` should always be taken.
  // Similarly in the second one it should never be taken.
  br->getRegion(0)->getFirstBlock()->inlineBefore(br);
  cr->getRegion(1)->getFirstBlock()->inlineBefore(cr);

  br->erase();
  cr->erase();
  return true;
}

}
