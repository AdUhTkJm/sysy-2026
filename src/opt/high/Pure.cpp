#include "Common.h"

namespace opt {

static bool walk(Op *parent) {
  bool impure = false;
  for_ops_in(parent, {
    impure |= walk(op);
  });
  if (hasSideEffect(parent))
    impure = true;

  if (impure)
    parent->set<ImpureAttr>();
  return impure;
}

declare_pass(Pure) {
  walk(module, [](Op *op) { op->remove<ImpureAttr>(); });
  opt::walk(module);
}

}