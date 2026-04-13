#include "Common.h"

namespace opt {

declare_pass(Inline,
  void runImpl(FuncOp *func);
  bool inlinable(FuncOp *op);

  std::unordered_map<FuncOp*, bool> inlinableCache;
) {
  for (auto func : collectFunctions())
    runImpl(func);

  // Remove unreachable functions.
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

void Inline::runImpl(FuncOp *func) {
  for_all(CallOp, func) {
    auto callee = op->val();
    auto fn = cast<FuncOp>(callee->def);
    if (!inlinable(fn))
      continue;

    // Do the real inlining.
    Builder builder(op);
    Builder::Map map;
    Builder::OpMap opmap;

    // Replace arguments.
    // Note both function arguments and call arguments start from 1.
    for (unsigned i = 1; i < op->getNumOperands(); i++)
      map[fn->ret(i)] = op->val(i);

    auto region = fn->getRegion();
    assert(region->getNumBlocks() == 1);
    builder.copy(region->getFirstBlock(), map, opmap);

    // Replace user sites.
    auto mappedReturn = op->prevOp();
    auto ret = cast<ReturnOp>(mappedReturn);
    if (ret->getNumOperands() > 0)
      op->ret()->replaceAllUsesWith(ret->val());

    // Remove the copied return, which always lands before the call.
    mappedReturn->erase();

    // Remove the call.
    op->erase();
  }
}

bool Inline::inlinable(FuncOp *op) {
  if (auto it = inlinableCache.find(op); it != inlinableCache.end())
    return it->second;

  // The function should not be recursive.
  if (op->has<RecursiveAttr>())
    return inlinableCache[op] = false;

  // The function should only contain a single return at the end.
  if (collectOps<ReturnOp>(op).size() != 1 || !isa<ReturnOp>(op->getRegion()->getLastOp()))
    return inlinableCache[op] = false;

  // Too large.
  if (op->getRegion()->getNumOps() >= 200)
    return inlinableCache[op] = false;

  return inlinableCache[op] = true;
}

}
