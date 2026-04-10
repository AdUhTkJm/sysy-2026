#include "Common.h"

namespace opt {



declare_pass(RangedFold) {
  Builder builder;
  for_all(DivIOp) {
    auto l = op->val(0), r = op->val(1);
    auto vi = dyn_cast<IntOp>(r->def);
    if (!vi || __builtin_popcount(vi->value) != 1)
      continue;

    auto it = rangeResult.find(op);
    if (it == rangeResult.end())
      continue;

    auto env = it->second;
    if (!env.count(l) || env[l].lo < 0)
      continue;

    // Now this is `l / r` where `l` is always positive.
    // We can convert this into a logical right shift.
    builder.setBefore(op);
    auto i = builder.createInt(__builtin_ctz(vi->value));
    auto rsh = builder.create<RShiftOp>(i32)->with(l, i->ret());
    op->ret()->replaceAllUsesWith(rsh->ret());
    op->erase();
  };

  walk(module, [](Op *op) {
    auto it = rangeResult.find(op);
    if (it == rangeResult.end())
      return;

    auto env = it->second;
    Builder builder;
    for (auto v : op->getOperands()) {
      if (!env.count(v))
        continue;

      data::Interval i = env[v];
      if (i.lo != i.hi || isa<IntOp>(v->def))
        continue;

      builder.setBefore(op);
      auto vi = builder.createInt(i.lo);
      v->replaceAllUsesThat(vi->ret(), [&](Op *op) {
        return rangeResult.count(op) && env.data == rangeResult.at(op).data;
      });
    }
  });
}

}
