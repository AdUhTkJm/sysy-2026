#include "Common.h"

namespace opt {

declare_pass(LoadSubstitute,
  ArrayStoreOp *latestDependency(ArrayLoadOp *op);

  std::vector<ArrayStoreOp*> stores;
) {
  stores = collectOps<ArrayStoreOp>();
  for (auto store : stores) {
    if (!store->has<SubscriptAttr>())
      return;
  }

  for_all(ArrayLoadOp) {
    auto store = latestDependency(op);
  }
}

// Try to find the case where the entire iteration domain of `op`
// is dependent on the store.
ArrayStoreOp *LoadSubstitute::latestDependency(ArrayLoadOp *op) {
  // Get the domain.
  auto indvars = collectIndvarFrom(op);
  if (!indvars)
    return nullptr;

  for (auto store : stores) {
      
  }
}

}
