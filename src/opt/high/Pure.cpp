#include "Common.h"

namespace opt {

static bool markUnerasable(Op *op) {
  bool unerasable = false;
  for_ops_in(op) {
    bool sub = markUnerasable(op);
    if (!isTerminator(op) || isa<ReturnOp>(op))
      unerasable |= sub;
  }
  if (hasSideEffect(op))
    unerasable = true;

  if (isa<CallOp>(op)) {
    auto func = cast<FuncOp>(op->val()->def);
    unerasable |= func->has<NonIdempotentAttr>();
  }

  if (unerasable)
    op->set<UnerasableAttr>();
  return unerasable;
}

#define checklist(X) \
  X(ArrayLoadOp) X(ArrayStoreOp) X(LoadOp) X(StoreOp) X(ExternCallOp)
  
#define non_empty(Ty) \
  !collectOps<Ty>(func).empty() ||

declare_pass(Pure) {
  walk(module, [](Op *op) {
    op->remove<UnerasableAttr>();
    op->remove<NonIdempotentAttr>();
  });

  auto funcs = collectFunctions();
  auto cg = callGraph();
  for (auto func : funcs) {
    // If a function calls an external function (I/O),
    // or stores to/loads from anything, then it's not idempotent.
    if (checklist(non_empty) false)
      func->set<NonIdempotentAttr>();
  }
  
  // Propagate non-idempotency across functions.
  fixed(for (auto func : funcs) {
    bool impure = false;
    for (auto v : cg[func]) {
      if (v->has<NonIdempotentAttr>()) {
        impure = true;
        break;
      }
    }
    if (!func->has<NonIdempotentAttr>() && impure) {
      mark_changed;
      func->set<NonIdempotentAttr>();
    }
  });

  markUnerasable(module);
}

}
