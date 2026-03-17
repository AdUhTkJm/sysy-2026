#include "Common.h"
#include <algorithm>

namespace opt {

declare_local_pass(AddressMode) {
  Builder builder;
  std::set<Op*> clean;

  for_all(LdrOp, func) {
    if (op->value != 0)
      continue;

    auto base = op->val()->def;
    auto ty = op->ret()->type;

    if (isa<AddXOp>(base)) {
      auto l = builder.replace<LdrLslOp>(op, ty)->with(base->val(0), base->val(1));
      l->value = 0;

      clean.insert(base);
      continue;
    }

    if (auto lsl = dyn_cast<AddXLslOp>(base); lsl && (1 << lsl->value) == asmSize(ty)) {
      auto l = builder.replace<LdrLslOp>(op, ty)->with(base->val(0), base->val(1));
      l->value = lsl->value;

      clean.insert(base);
      continue;
    }
  }

  for_all(StrOp, func) {
    if (op->value != 0)
      continue;
    
    auto base = op->val()->def;
    auto value = op->val(1);

    if (isa<AddXOp>(base)) {
      auto l = builder.replace<StrLslOp>(op)->with(base->val(0), base->val(1), value);
      l->value = 0;

      clean.insert(base);
      continue;
    }

    if (auto lsl = dyn_cast<AddXLslOp>(base); lsl && (1 << lsl->value) == asmSize(value->type)) {
      auto l = builder.replace<StrLslOp>(op)->with(base->val(0), base->val(1), value);
      l->value = lsl->value;

      clean.insert(base);
      continue;
    }
  }

  for (auto op : clean) {
    if (std::none_of(op->getResults().begin(), op->getResults().end(), [](const Value *v) { return v->used(); }))
      op->erase();
  }
}

}
