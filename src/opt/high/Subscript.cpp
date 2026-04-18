#include "Common.h"
#include "../../utils/presburger/AffineFunction.h"
#include <optional>

namespace opt {

declare_pass(Subscript,
  using Liveset = std::set<LoadOp*>;

  void markSubscript();
  std::optional<pres::AffineFunction> getFunction(Value *v);

  std::vector<Indvar> outer;
) {
  markSubscript();
}

void Subscript::markSubscript() {
  std::vector<Op*> ops;
  auto loads = collectOps<ArrayLoadOp>();
  auto stores = collectOps<ArrayStoreOp>();
  std::copy(loads.begin(), loads.end(), std::back_inserter(ops));
  std::copy(stores.begin(), stores.end(), std::back_inserter(ops));

  for (auto op : ops) {
    auto indvars = collectIndvarFrom(op);
    if (!indvars)
      continue;

    outer = *indvars;
    std::vector<pres::AffineFunction> subscripts;
    auto subscriptCount = op->getNumOperands();
    if (isa<ArrayStoreOp>(op))
      subscriptCount--;

    bool exit = false;
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

std::optional<pres::AffineFunction> Subscript::getFunction(Value *v) {
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
