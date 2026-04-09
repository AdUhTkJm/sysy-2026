#include "Common.h"

namespace opt {

declare_local_pass(Flatten,
  void flatten(Op *op);
  void flattenIf(Op *op);
  void flattenDoWhile(Op *op);
) {
  auto region = func->getRegion();
  fixed(for (auto bb : *region) {
    for (auto op : *bb) {
      if (isa<IfOp>(op)) {
        flattenIf(op); mark_changed;
        break;
      }

      if (isa<DoWhileOp>(op)) {
        flattenDoWhile(op); mark_changed;
        break;
      }
    }
  });

  Builder builder;
  for (auto bb : *region) {
    if (bb->getNumOps() == 0) {
      builder.setToStart(bb);
      builder.create<UnreachableOp>();
    }
  }
}

void Flatten::flatten(Op *op) {
  if (isa<IfOp>(op))
    flattenIf(op);

  if (isa<DoWhileOp>(op))
    flattenDoWhile(op);
}

void Flatten::flattenIf(Op *op) {
  auto bb = op->getParentBlock();
  auto region = bb->getParentRegion();
  auto ifso = region->insertAfter(bb);
  auto ifnot = region->insertAfter(ifso);
  auto end = region->insertAfter(ifnot);
  bb->splitOpsAfter(end, op);
  
  // Now everything after `end` should refer to the block arguments in `end`.
  for (auto result : op->getResults()) {
    auto arg = end->pushArgument(result->type);
    result->replaceAllUsesWith(arg);
  }

  auto l = op->getRegion(0), r = op->getRegion(1);
  l->getFirstBlock()->inlineToEnd(ifso);
  r->getFirstBlock()->inlineToEnd(ifnot);

  Builder builder;
  // Replace the final yield into a jump.
  if (auto last = ifso->getLastOp(); isa<YieldOp>(last)) {
    auto jmp = builder.rename<JumpOp>(last);
    jmp->target = end;
  }

  if (auto last = ifnot->getLastOp(); isa<YieldOp>(last)) {
    auto jmp = builder.rename<JumpOp>(last);
    jmp->target = end;
  }

  // We need to introduce a branch at the end of the first block.
  builder.setToEnd(bb);
  auto br = builder.create<BranchOp>()->with(op->val());
  br->target = ifso;
  br->other = ifnot;

  // Clear this IfOp.
  op->erase();
}

void Flatten::flattenDoWhile(Op *op) {
  auto bb = op->getParentBlock();
  auto region = bb->getParentRegion();
  auto body = region->insertAfter(bb);
  auto end = region->insertAfter(body);

  bb->splitOpsAfter(end, op);
  op->getRegion()->getFirstBlock()->inlineToEnd(body);

  // The block argument of `body` only affects ops in `body`;
  // The block argument of `end` affects everything else.
  for (auto result : op->getResults()) {
    auto bodyArg = body->pushArgument(result->type);
    auto endArg = end->pushArgument(result->type);
    result->replaceAllUsesThat(bodyArg, [&](Op *op) {
      return op->inside(body);
    });
    result->replaceAllUsesWith(endArg);
  }

  // Now we must turn the ConditionOp into a branch.
  Builder builder;
  if (auto last = body->getLastOp(); isa<ConditionOp>(last)) {
    auto br = builder.rename<BranchOp>(last);
    br->target = body;
    br->other = end;
  }
  
  // The beginning block must jump to the body with given arguments.
  builder.setToEnd(bb);
  auto jmp = builder.create<JumpOp>()->with(op->getOperands());
  jmp->target = body;

  // Clear this WhileOp.
  op->erase();
}

}