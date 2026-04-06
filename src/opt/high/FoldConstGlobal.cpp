#include "Common.h"

namespace opt {

declare_pass(FoldConstGlobal) {
  std::set<GlobalOp*> stored;
  std::set<GlobalOp*> eligible;
  walk(module, [&](Op *op) {
    if (isa<StoreOp>(op)) {
      auto addr = op->val()->def;
      if (isa<GetGlobalOp>(addr))
        stored.insert(cast<GlobalOp>(addr->val()->def));
    }
    if (isa<LoadOp>(op)) {
      auto addr = op->val()->def;
      if (isa<GetGlobalOp>(addr))
        eligible.insert(cast<GlobalOp>(addr->val()->def));
    }
  });

  Builder builder;
  for (auto op : eligible) {
    if (stored.count(op))
      continue;

    auto attr = op->get<ConstIArrAttr>();
    if (!attr)
      continue;

    assert(attr->value.size() == 1);
    int vi = attr->value[0];

    for (auto get : op->ret()->getUses()) {
      assert(isa<GetGlobalOp>(get));
      for (auto load : get->ret()->getUses()) {
        builder.setBefore(load);
        assert(isa<LoadOp>(load));
        auto v = builder.createInt(vi);
        load->ret()->replaceAllUsesWith(v->ret());
      }
    }
  }
}

}
