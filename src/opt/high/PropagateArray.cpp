#include "Common.h"
#include <queue>

namespace opt {

// For all array arguments with type `ptr`, we replace them with the calling array.
// This might involve specializing the function.
declare_pass(PropagateArray) {
  // Since we might create new functions, we need to recalculate the set of FuncOp
  // every time.
  // Precondition: there are no recursive function calls (TODO)
  unsigned index = 0;
  fixed(
  // Toposort the call graph.
  CallGraph cg = callGraph();
  
  std::unordered_map<FuncOp*, int> indegree;
  for (const auto &[fn, _] : cg)
    indegree[fn] = 0;

  for (const auto &[fn, vec] : cg) {
    for (auto op : vec)
      indegree[op]++;
  }

  std::queue<FuncOp*> q;
  for (const auto &[fn, deg] : indegree) {
    if (deg == 0 && fn->name == "main")
      q.push(fn);
    else if (deg == 0)
      fn->erase();
  }

  std::set<FuncOp*> visited;
  while (!q.empty()) {
    auto func = q.front();
    q.pop();
    if (visited.count(func))
      continue;
    visited.insert(func);

    std::map<std::pair<FuncOp*, Value*>, FuncOp*> waiting;
    for_all(CallOp, func) {
      for (unsigned i = 1; i < op->getNumOperands(); i++) {
        const auto arg = op->val(i);
        if (arg->type->kind != Type::ptr)
          continue;

        auto callee = cast<FuncOp>(op->val(0)->def);
        auto g = arg->def;
        // Precondition: every array becomes a global array.
        assert(g && isa<GetGlobalOp>(g));

        auto global = g->val(0);

        FuncOp *fn;
        if (auto it = waiting.find({ callee, global }); it != waiting.end())
          fn = it->second;
        else {
          Builder builder;
          Builder::Map map;

          builder.setAfter(callee);
          fn = cast<FuncOp>(builder.clone(callee));
          fn->name = "__p" + std::to_string(index++) + "_" + callee->name;

          // After cloning, replace the i'th argument to `val`.
          builder.setToStart(fn->getRegion());
          auto newg = builder.create<GetGlobalOp>(g->ret()->type)->with(global);
          fn->getArg(i - 1)->replaceAllUsesWith(newg->ret());
          fn->removeResult(i - 1);
        }

        // For the call instance, remove the i'th argument (i+1'th operand),
        // and change the first operand to call `func` instead.
        op->setOperand(0, fn->ret());
        op->removeOperand(i);
        mark_changed;

        waiting[{ callee, global }] = fn;

        if (--indegree[callee] == 0)
          q.push(fn);
      }
    }
  })

  for (auto fn : collectFunctions()) {
    if (!fn->ret()->used() && fn->name != "main")
      fn->erase();
  }
}

}