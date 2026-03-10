#include "Common.h"
#include "../../utils/SCC.h"

namespace opt {

// For all array arguments with type `ptr`, we replace them with the calling array.
// This might involve specializing the function.
declare_pass(PropagateArray,
  unsigned index = 0;
  std::map<std::pair<FuncOp*, Value*>, FuncOp*> specs;

  bool specialize(FuncOp *func, unsigned i, Value *v);
) {
  // Try to specialize from main.
  fixed(std::vector<FuncOp*> queue { findMain() };
  std::set<FuncOp *> visited;
  while (!queue.empty()) {
    auto func = queue.back();
    queue.pop_back();
    if (visited.count(func))
      continue;
    visited.insert(func);

    // Collect all functions called.
    for_all(CallOp, func) {
      for (unsigned i = 1; i < op->getNumOperands(); i++) {
        const auto *arg = op->val(i);
        if (arg->type->kind != Type::ptr)
          continue;

        auto callee = cast<FuncOp>(op->val()->def);
        auto global = cast<GetGlobalOp>(arg->def)->val();
        if (specialize(callee, i, global)) {
          mark_changed;
          queue.push_back(callee);
        }
      }
    }

    if (__changed)
      break;
  })

  // Compute reachable functions.
  std::set<FuncOp*> visited;
  std::vector<FuncOp*> queue { findMain() };
  auto cg = callGraph();

  while (!queue.empty()) {
    auto fn = queue.back();
    queue.pop_back();
    if (visited.count(fn))
      continue;
    visited.insert(fn);
    
    for (auto x : cg[fn])
      queue.push_back(x);
  }

  auto funcs = collectFunctions();
  for (auto w : funcs) {
    if (!visited.count(w))
      w->getRegion()->prepareErase();
  }
  for (auto w : funcs) {
    if (!visited.count(w))
      w->erase();
  }
}

bool PropagateArray::specialize(FuncOp *func, unsigned i, Value *v) {
  // Collect all calls to the function with the required argument,
  // both from inside and outside of this function.
  std::vector<CallOp*> calls;
  walk(module, [&](Op *op) {
    auto call = dyn_cast<CallOp>(op);
    if (!call || call->val(0) != func->ret())
      return;

    auto get = dyn_cast<GetGlobalOp>(call->val(i)->def);
    if (!get || get->val(0)->def != v->def)
      return;

    calls.push_back(call);
  });

  // Create (or retrieve) the specialized version of this function.
  FuncOp *spec;
  bool changed = false;
  auto key = std::make_pair(func, v);
  if (auto it = specs.find(key); it != specs.end())
    spec = it->second;
  else {
    Builder builder;
    builder.setAfter(func);

    spec = cast<FuncOp>(builder.clone(func));
    spec->name = "_" + std::to_string(index++) + func->name;

    // Replace the i'th argument with the global `v`.
    builder.setToStart(spec->getRegion());
    auto newg = builder.create<GetGlobalOp>(Type::pointer(v->type))->with(v);
    spec->ret(i)->replaceAllUsesWith(newg->ret());
    spec->removeResult(i);
    specs[key] = spec;
    changed = true;
  }

  // Replace the calls. 
  for (auto op : calls) {
    op->setOperand(0, spec->ret());
    op->removeOperand(i);
  }

  // Specially, if the function calls itself, then the calls to itself has not 
  // been recorded in `calls`, because it is generated afterwards.
  // Therefore we deal with it here.
  for_all(CallOp, spec) {
    if (op->val() != spec->ret())
      continue;

    op->removeOperand(i);
  }

  return changed;
}

}
