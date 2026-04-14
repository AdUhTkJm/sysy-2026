#include "Common.h"

namespace opt {

declare_pass(AccessMode) {
  Builder builder;

  for_all(LoadOp) {
    auto addr = op->val();
    for (auto x = op->nextOp(); x; x = x->nextOp()) {
      auto add = dyn_cast<AddLOp>(x);
      if (!add || x->val(0) != addr)
        continue;

      // Now this must be `addr + 'a`.
      auto vi = dyn_cast<IntOp>(x->val(1)->def);
      if (!vi || vi->value >= 256 || vi->value < -256)
        continue;

      // Now combine them.
      builder.setAfter(op);
      auto ldr = builder.create<LdrPostIncrOp>(i32, i64)->with(addr);
      ldr->value = vi->value;
      op->ret()->replaceAllUsesWith(ldr->ret(0));
      add->ret()->replaceAllUsesWith(ldr->ret(1));
      break;
    }
  }
  
  // Similarly for store.
  for_all(StoreOp) {
    auto addr = op->val(0);
    for (auto x = op->nextOp(); x; x = x->nextOp()) {
      auto add = dyn_cast<AddLOp>(x);
      if (!add || x->val(0) != addr)
        continue;

      auto vi = dyn_cast<IntOp>(x->val(1)->def);
      if (!vi || vi->value >= 256 || vi->value < -256)
        continue;

      builder.setAfter(op);
      auto str = builder.create<StrPostIncrOp>(i64)->with(addr, op->val(1));
      str->value = vi->value;
      add->ret()->replaceAllUsesWith(str->ret());
      break;
    }
  }
}

}
