#include "Common.h"

namespace opt {

int computeOffset(Op *op) {
  int result = 0;
  auto dims = op->val(0)->def/*GetGlobal*/->val(0)->def/*Global*/->get<DimAttr>()->dims;
  dims.push_back(1);
  for (unsigned i = 1; i + 1 < op->getNumOperands(); i++) {
    auto w = dyn_cast<IntOp>(op->val(i)->def);
    if (!w)
      return -1;

    result = (result + w->value) * dims[i];
  }
  return result;
}

declare_pass(InlineGlobalStore) {
  auto main = findMain();
  // We go through the stores and inline them if they access a global array.
  // Note that the attributes are immutable, and hence we need to store it elsewhere.
  std::map<GlobalOp*, std::vector<int>> inlined;
  std::map<GlobalOp*, std::vector<float>> inlinedF;
  bool doArray = true, doScalar = true;
  for (auto op = main->getRegion()->getFirstOp(); op && (doArray || doScalar);) {
    auto next = op->nextOp();
    if (isa<ArrayStoreOp>(op)) {
      auto val = op->val(op->getNumOperands() - 1), addr = op->val(0);
      if (auto vi = dyn_cast<IntOp>(val->def); vi && isa<GetGlobalOp>(addr->def)) {
        auto offset = computeOffset(op);
        if (offset == -1)
          goto end;

        auto global = cast<GlobalOp>(addr->def->val(0)->def);
        auto ty = global->ret()->type;
        if (ty == i32) {
          if (!inlined.count(global))
            inlined.insert(inlined.begin(), std::make_pair(global, global->get<ConstIArrAttr>()->value));
          inlined[global][offset] = vi->value;
        } else {
          assert(ty == f32);
          if (!inlinedF.count(global))
            inlinedF.insert(inlinedF.begin(), std::make_pair(global, global->get<ConstFArrAttr>()->value));
          inlinedF[global][offset] = vi->value;
        }
        op->erase();
      }
    }
    if (isa<StoreOp>(op)) {
      auto val = op->val(1)->def, addr = op->val(0)->def;
      if (auto vi = dyn_cast<IntOp>(val); vi && isa<GetGlobalOp>(addr)) {
        auto global = cast<GlobalOp>(addr->val(0)->def);
        inlined[global] = { vi->value };
        op->erase();
      }
    }

    if (contains<ArrayLoadOp>(op))
      doArray = false;
    if (contains<LoadOp>(op))
      doScalar = false;
    end: op = next;
  }

  // Rewrite attributes back.
  for (auto &[op, vec] : inlined) {
    op->remove<ConstIArrAttr>();
    op->set<ConstIArrAttr>(vec);
  }
  for (auto &[op, vec] : inlinedF) {
    op->remove<ConstFArrAttr>();
    op->set<ConstFArrAttr>(vec);
  }
}

}