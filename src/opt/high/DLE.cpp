#include "Common.h"
#include "../../utils/presburger/AffineFunction.h"
#include <optional>

namespace opt {

declare_pass(DLE,
  using Liveset = std::set<LoadOp*>;

  void markSubscript();
  std::optional<pres::AffineFunction> getFunction(Value *v);

  struct Record {
    Value *indvar, *limit;
  };
  std::vector<Record> outer;
) {
  markSubscript();
}

void DLE::markSubscript() {
  std::vector<Op*> ops;
  auto loads = collectOps<ArrayLoadOp>();
  auto stores = collectOps<ArrayStoreOp>();
  std::copy(loads.begin(), loads.end(), std::back_inserter(ops));
  std::copy(stores.begin(), stores.end(), std::back_inserter(ops));

  for (auto op : ops) {
    outer.clear();
    bool exit = false;
    for (Op *x = op; !isa<FuncOp>(x); x = x->getParentOp()) {
      if (auto loop = dyn_cast<DoWhileOp>(x)) {
        Record rec;
        rec.indvar = indvar(loop, &rec.limit);
        if (!rec.indvar) {
          exit = true;
          break;
        }
        auto incr = increment(rec.indvar);
        if (!incr || !isa<IntOp>(incr->def) || cast<IntOp>(incr->def)->value != 1) {
          exit = true;
          break;
        }

        outer.push_back(rec);
      }
    }
    if (exit)
      continue;
    // Permute it so that the outermost loop appears first in the vector.
    std::reverse(outer.begin(), outer.end());
    
    std::vector<pres::AffineFunction> subscripts;

    auto subscriptCount = op->getNumOperands();
    if (isa<ArrayStoreOp>(op))
      subscriptCount--;
    for (unsigned i = 1; i < subscriptCount; i++) {
      auto affine = getFunction(op->val(i));
      if (!affine) {
        exit = true;
        break;
      }

      subscripts.push_back(*affine);
    }
    if (exit)
      continue;

    op->set<SubscriptAttr>(subscripts);
  }
}

std::optional<pres::AffineFunction> DLE::getFunction(Value *v) {
  for (unsigned i = 0; i < outer.size(); i++) {
    if (v == outer[i].indvar)
      return pres::domain(outer.size())[i];
  }

  auto op = v->def;
  if (auto vi = dyn_cast<IntOp>(op))
    return pres::constant(outer.size(), vi->value);
  
  if (isa<AddIOp>(op)) {
    auto l = getFunction(op->val(0));
    auto r = getFunction(op->val(1));
    if (!l || !r)
      return std::nullopt;

    return *l + *r;
  }

  if (isa<SubIOp>(op)) {
    auto l = getFunction(op->val(0));
    auto r = getFunction(op->val(1));
    if (!l || !r)
      return std::nullopt;

    return *l - *r;
  }

  return std::nullopt;
}

}
