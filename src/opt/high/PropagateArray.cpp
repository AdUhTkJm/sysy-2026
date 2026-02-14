#include "Common.h"

namespace opt {

// For all array arguments with type `ptr`, we replace them with the calling array.
// This might involve specializing the function.
declare_pass(PropagateArray) {
  // Since we might create new functions, we need to recalculate the set of FuncOp
  // every time.
  fixed(for_all(FuncOp) {
  for (unsigned i = 0; i + 1 < op->getNumResults(); i++) {
    const auto arg = op->getArg(i);
    if (arg->type->kind != Type::ptr)
      continue;

    // Get all calling points and find out the supplied pointer.
    auto calls = op->ret()->getUses();
    std::map<Value*, std::vector<Op*>> pointerMap;
    for (auto call : calls) {
      assert(isa<CallOp>(call));
      pointerMap[call->val(i + 1)].push_back(call);
    }

    // For each different value, specialize the function.
    for (const auto &[i, data] : data::enumerate(pointerMap)) {
      auto &[addr, vec] = data;
      auto g = addr->def;
      if (!g || !isa<GetGlobalOp>(g))
        continue;

      auto global = g->val(0);

      Builder builder;
      Builder::Map map;

      builder.setAfter(op);
      auto func = cast<FuncOp>(builder.clone(op));
      if (pointerMap.size() != 1)
        func->name = "__p" + std::to_string(i) + "_" + op->name;

      // After cloning, replace the i'th argument to `val`.
      builder.setToStart(func->getRegion());
      auto newg = builder.create<GetGlobalOp>(g->ret()->type)->with(global);
      func->getArg(i)->replaceAllUsesWith(newg->ret());
      func->removeResult(i + 1);

      // For all call instances, remove the i'th argument (i+1'th operand),
      // and change the first operand to call `func` instead.
      for (auto call : vec) {
        call->setOperand(0, func->ret());
        call->removeOperand(i + 1);
      }

      mark_changed;
    }

    if (!op->ret()->used()) {
      op->erase();
      break;
    }
  }})
}

}