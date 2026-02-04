#include "Common.h"
#include <algorithm>

namespace opt {

Pass *makePure(ir::ModuleOp *module);

declare_pass(HighDCE) {
  makePure(module)->run();
  fixed(
    walk<Postorder>(module, [&](Op *op) {
      if (std::all_of(op->getResults().begin(), op->getResults().end(), [](Value *v){
        return !v->used();
      }) && !op->has<ImpureAttr>()) {
        op->erase();
        mark_changed;
      }
    });
  );
  walk(module, [](Op *op) { op->remove<ImpureAttr>(); });
}

}
