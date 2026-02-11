#include "Common.h"

namespace opt {

declare_local_pass(DestroyPhi,
  void splitCriticalEdge(Region *region);
  void lowerPhi(Block *bb);
) {
  auto region = func->getRegion();
  
  region->updatePreds();
  splitCriticalEdge(region);

  for (auto bb : *region)
    lowerPhi(bb);
}

void DestroyPhi::splitCriticalEdge(Region *region) {
  std::vector<std::pair<Block*, Block*>> edges;
  for (auto bb : *region) {
    for (auto succ : bb->succs) {
      if (succ->preds.size() <= 1)
        continue;

      // The edge from `bb` to `succ` is a critical edge,
      // and has to be splitted.
      edges.emplace_back(bb, succ);
    }
  }

  for (auto [bb, succ] : edges) {
    auto mid = region->insertAfter(bb);
    bb->rewire(succ, mid);

    Builder builder;
    builder.setToEnd(mid);
    builder.create<BOp>()->target = succ;

    // Fix phi nodes at the beginning of `succ`.
    for (auto op : *succ) {
      auto phi = dyn_cast<PhiOp>(op);
      if (!phi)
        break;

      phi->replaceIncoming(bb, mid);
    }
  }

  // The topology has changed, and we need to manually update that.
  region->updatePreds();
}

using PhiEdge = std::pair<Reg, Reg>;

void DestroyPhi::lowerPhi(Block *bb) {
  auto phis = collectOps<PhiOp>(bb);

  for (auto pred : bb->preds) {
    Builder builder(pred->getLastOp());
    const auto &emitCopy = [&](Reg src, Reg dst) {
      auto rd = createAssignedRd(builder, src);

      auto wr = builder.create<WriteRegOp>()->with(rd->ret());
      wr->reg = dst;
    };

    // Maps dst to src.
    std::unordered_map<Reg, Reg> copy;
    std::set<Reg> srcs;

    for (auto phi : phis) {
      auto src = assignment[phi->incomingFrom(pred)];
      auto dst = assignment[phi->ret()];
      if (src == dst)
        continue;

      copy[dst] = src;
      srcs.insert(src);
    }

    // First deal with acyclic parts, and remove them from `copy`.
    fixed(for (auto it = copy.begin(); it != copy.end(); ) {
      auto [dst, src] = *it;

      if (!copy.count(src)) {
        emitCopy(src, dst);
        it = copy.erase(it);
        mark_changed;
      } else
        ++it;
    });

    // Now the ones in `copy` form cycles.
    while (!copy.empty()) {
      auto [start, _] = *copy.begin();
      std::vector<Reg> cycle;
      Reg cur = start;
      do {
        cycle.push_back(cur);
        cur = copy[cur];
      } while (cur != start);

      // Remove the cycle with the scratch register.
      emitCopy(copy[start], scratch);
      for (int i = (int)cycle.size() - 1; i > 0; --i)
        emitCopy(copy[cycle[i]], cycle[i]);
      emitCopy(scratch, start);

      // Delete the cycle from the registers.
      for (auto r : cycle)
        copy.erase(r);
    }
  }

  // The phis are now destroyed. Rewrite them to reads.
  Builder builder;
  for (auto phi : phis) {
    builder.setBefore(phi);
    auto rd = createAssignedRd(builder, assignment[phi->ret()]);
    phi->ret()->replaceAllUsesWith(rd->ret());
    phi->erase();
  }
}

}
