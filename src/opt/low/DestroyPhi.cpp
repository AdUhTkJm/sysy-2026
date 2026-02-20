#include "Common.h"
#include <queue>

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
      if (succ->preds.size() <= 1 && bb->succs.size() <= 1)
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
    const auto &emitCopy = [&](Reg src, Reg dst, const Type *ty) {
      auto rd = createAssignedRd(builder, src, ty);

      auto wr = builder.create<WriteRegOp>()->with(rd->ret());
      wr->reg = dst;
    };

    // Maps dst to src.
    std::unordered_map<Reg, std::pair<Reg, const Type*>> copy;
    std::set<Reg> srcs;

    for (auto phi : phis) {
      auto src = assignment[phi->incomingFrom(pred)];
      auto dst = assignment[phi->ret()];
      if (src == dst)
        continue;

      copy[dst] = { src, phi->ret()->type };
      srcs.insert(src);
    }

    // First deal with acyclic parts, and remove them from `copy`.
    std::unordered_map<Reg, int> indegree;
    for (auto &[dst, pair] : copy)
      indegree[dst] = 0;

    for (auto &[dst, pair] : copy) {
      auto src = pair.first;
      if (copy.count(src))
        indegree[src]++;
    }

    // Toposort.
    std::queue<Reg> q;
    for (auto &[r, deg] : indegree) {
      if (deg == 0)
        q.push(r);
    }

    while (!q.empty()) {
      Reg dst = q.front();
      q.pop();

      if (!copy.count(dst))
        continue;

      auto [src, ty] = copy[dst];
      emitCopy(src, dst, ty);

      copy.erase(dst);

      if (copy.count(src)) {
        if (--indegree[src] == 0)
          q.push(src);
      }
    }

    // Now the ones in `copy` form cycles.
    while (!copy.empty()) {
      auto [start, result] = *copy.begin();
      std::vector<std::pair<Reg, const Type*>> cycle;
      Reg cur = start;
      const Type *ty = result.second;
      do {
        cycle.emplace_back(cur, ty);
        auto [c, t] = copy[cur];
        cur = c; ty = t;
      } while (cur != start);

      // Remove the cycle with the scratch register.
      emitCopy(copy[start].first, scratch, ty);
      for (int i = (int) cycle.size() - 1; i > 0; --i) {
        auto [dst, ty] = cycle[i];
        emitCopy(copy[dst].first, dst, ty);
      }
      emitCopy(scratch, start, ty);

      // Delete the cycle from the registers.
      for (auto r : cycle)
        copy.erase(r.first);
    }
  }

  // The phis are now destroyed. Rewrite them to reads.
  Builder builder;
  for (auto phi : phis) {
    builder.setBefore(phi);
    auto rd = createAssignedRd(builder, assignment[phi->ret()], phi->ret()->type);
    phi->ret()->replaceAllUsesWith(rd->ret());
    phi->erase();
  }
}

}
