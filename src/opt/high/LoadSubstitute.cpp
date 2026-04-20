#include "Common.h"
#include "../../utils/presburger/IntegerRelation.h"
#include "../../utils/presburger/Expr.h"

using namespace pres;

namespace opt {

using Row = IntegerRelation::Row;

struct LoopBound {
  pres::Expr low, high;
};

declare_local_pass(LoadSubstitute,
  ArrayStoreOp *latestDependency(ArrayLoadOp *op);

  void addLexLess(PresburgerRelation &rel, IntegerRelation base, Op *dep, Op *sink, unsigned depDims, unsigned srcDims);
  void addBounds(IntegerRelation &rel, const std::vector<LoopBound> &loopBound, unsigned dims, unsigned offset, unsigned until = -1u);

  std::vector<ArrayStoreOp*> stores;
  std::unordered_map<Op*, unsigned> lexIndex;
) {
  stores = collectOps<ArrayStoreOp>(func);
  for (auto store : stores) {
    if (!store->has<SubscriptAttr>())
      return;
  }

  for_all(ArrayLoadOp, func) {
    auto store = latestDependency(op);
  }
}

unsigned findCommonLoopDepth(Op *x, Op *y) {
  std::vector<Op*> loopParents;
  for (auto *runner = x->getParentOp(); !isa<ModuleOp>(runner); runner = runner->getParentOp()) {
    if (isa<DoWhileOp>(runner))
      loopParents.push_back(runner);
  }

  for (auto *runner = y->getParentOp(); !isa<ModuleOp>(runner); runner = runner->getParentOp()) {
    if (!isa<DoWhileOp>(runner))
      continue;

    auto it = std::find(loopParents.begin(), loopParents.end(), runner);
    if (it != loopParents.end())
      return loopParents.end() - it;
  }
  return 0;
}

/// Adds constraints `dep < sink` (lexicographically), based on the constraints
/// recorded in `base`. Assumes that 0.. srcDims are for `sink` and the rest are
/// for `deps`.
void LoadSubstitute::addLexLess(PresburgerRelation &rel, IntegerRelation base, Op *dep, Op *sink, unsigned depDims, unsigned srcDims) {
  unsigned depth = findCommonLoopDepth(sink, dep);
  for (unsigned i = 0; i <= std::max(depDims, srcDims); i++) {
    if (i >= depth) {
      // Now all common loop indices are already exhausted.
      // If `dep` goes lexically before `sink` in the sink program,
      // it will always be lexicographically smaller than `sink`.
      // Hence the whole `depend` will be added.
      if (lexIndex[dep] < lexIndex[sink])
        rel += base;

      // Otherwise, no instance of `dep` is lexicographically smaller
      // than sink, and we need to do nothing.
      // Special case: if i==0, then `rel` should be set to empty.
      else if (i == 0)
        rel = PresburgerRelation();
      
      // In both cases, there's no need to add further constraints.
      break;
    }

    auto copy = base;

    // v_d[i] < v_s[i].
    // i.e. v_s[i] - v_d[i] - 1 >= 0.
    copy.add(var[i] - var[i + srcDims] >= 1);
    rel += copy;

    // Add constraint v_d[i] = v_s[i] for the next iteration.
    base.add(var[i] - var[i + srcDims] == 0);
  }
}

void LoadSubstitute::addBounds(
  IntegerRelation &rel, const std::vector<LoopBound> &loopBound, unsigned dims, unsigned offset, unsigned until
) {
  // for (auto [i, bound] : data::enumerate(loopBound)) {
  //   if (i >= until)
  //     break;
  //   auto [low, high] = bound;
  //   // We must insert `dims(sinkMap)` elements at front.
  //   auto coeffLow = low.extractCoefficients(dims);
  //   auto coeffHigh = high.extractCoefficients(dims);
  //   // Adjust as done above.
  //   coeffHigh[i] -= 1;
  //   coeffHigh.back() -= 1;
  //   for (auto &c : coeffLow)
  //     c *= -1;
  //   coeffLow[i] += 1;

  //   Row shifted(dims + 1);

  //   for (unsigned i = 0; i < dims - offset; i++)
  //     shifted[i + offset] = coeffLow[i];
  //   shifted.back() = coeffLow.back();
  //   rel.addInequality(shifted);
    
  //   for (unsigned i = 0; i < dims - offset; i++)
  //     shifted[i + offset] = coeffHigh[i];
  //   shifted.back() = coeffHigh.back();
  //   rel.addInequality(shifted);
  // }
}

// Try to find the case where the entire iteration domain of `op`
// is dependent on the store.
ArrayStoreOp *LoadSubstitute::latestDependency(ArrayLoadOp *op) {
  // Get the domain.
  auto indvars = collectIndvarFrom(op);
  if (!indvars)
    return nullptr;

  auto srcSub = op->get<SubscriptAttr>();
  for (auto store : stores) {
    // They must access the same array.
    if (op->val() != store->val())
      continue;

    auto depSub = store->get<SubscriptAttr>();
    unsigned srcDims = srcSub->subscripts[0].dimension(), depDims = depSub->subscripts[0].dimension();
    Space s { srcDims, depDims, 0, 0 };
    IntegerRelation rel(s);
    
    // For dependency, subscripts must be equal.
    assert(srcSub->subscripts.size() == depSub->subscripts.size());
    for (const auto &[i, src] : data::enumerate(srcSub->subscripts)) {
      const auto &dep = depSub->subscripts[i];
      IntegerRelation::Row row(s.getNumCols());
      for (unsigned i = 0; i < srcDims; i++)
        row[i] = src[i];
      for (unsigned i = 0; i < depDims; i++)
        row[i + srcDims] = -dep[i];
      row.back() = src.constant() - dep.constant();
      rel.addEquality(row);
    }

    // `depSub` must be lexicographically before `srcSub`.

  }
}

}
