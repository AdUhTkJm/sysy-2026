#include "Common.h"

namespace opt {

declare_local_pass(AddressMode) {
  Builder builder;
  std::vector<Op*> clean;

  for_all(LdrOp, func) {
    auto base = op->val()->def;
    auto ty = op->ret()->type;

    if (isa<AddXOp>(base)) {
      auto l = builder.replace<LdrLslOp>(op, ty)->with(base->val(0), base->val(1));
      l->value = 0;

      clean.push_back(base);
      continue;
    }

    
  }
}

}
