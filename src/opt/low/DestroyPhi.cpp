#include "Common.h"
#include <algorithm>
#include <queue>

namespace opt {

declare_local_pass(DestroyPhi,
  void splitCriticalEdge(Region *region);
  void lowerPhi(Block *bb);
  void spill(Value *v);
) {
  auto region = func->getRegion();
  
  region->updatePreds();
  splitCriticalEdge(region);

  for (auto bb : *region)
    lowerPhi(bb);

  // The phis are now destroyed. Rewrite them to reads.
  Builder builder;
  for_all(PhiOp, region->getParentOp()) {
    builder.setBefore(op);

    auto v = op->ret();
    auto reg = assignment[v];

    if (reg < reg_end) {
      auto rd = createAssignedRd(builder, assignment[v], v->type);
      v->replaceAllUsesWith(rd->ret());
    } else {
      auto alloca = (AllocaOp *) (unsigned long) reg;
      for (auto it = v->getUses().begin(); !v->getUses().empty();) {
        Op *use = *it;
        builder.setBefore(use);
        auto ld = builder.create<LdrOp>(v->type)->with(alloca->ret());
        assignment[ld->ret()] = scratch[use->getOperandIndex(v)];
        use->replaceOperand(v, ld->ret());
        it = v->getUses().begin();
      }
    }
    op->erase();
  }

  for (auto bb : *region) {
    std::vector<Value*> toSpill;
    for (auto op : *bb) {
      std::copy_if(
        op->getResults().begin(), op->getResults().end(),
        std::back_inserter(toSpill), [&](Value *v) {
          return assignment[v] > reg_end;
        }
      );
    }
    for (auto v : toSpill)
      spill(v);
  }
}

void DestroyPhi::spill(Value *v) {
  Builder builder;
  auto alloca = (AllocaOp *) (unsigned long) assignment[v];
  while (!v->getUses().empty()) {
    auto it = v->getUses().begin();
    Op *use = *it;
    builder.setBefore(use);
    auto ld = builder.create<LdrOp>(v->type)->with(alloca->ret());
    assignment[ld->ret()] = scratch[use->getOperandIndex(v)];
    use->replaceOperand(v, ld->ret());
  }

  builder.setAfter(v->def);
  builder.create<StrOp>()->with(alloca->ret(), v);
  assignment[v] = scratch[v->def->getResultIndex(v)];
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
      Value *rd;
      if (src < reg_end)
        rd = createAssignedRd(builder, src, ty)->ret();
      else {
        auto alloca = (AllocaOp *) (unsigned long) src;
        rd = builder.create<LdrOp>(ty)->with(alloca->ret())->ret();
        assignment[rd] = scratch[0];
      }

      if (dst < reg_end) {
        auto wr = builder.create<WriteRegOp>()->with(rd);
        wr->reg = dst;
      } else {
        auto alloca = (AllocaOp *) (unsigned long) dst;
        builder.create<StrOp>()->with(alloca->ret(), rd);
      }
    };

    // Maps dst to src.
    std::unordered_map<Reg, std::pair<Reg, const Type*>> copy;
    std::set<Reg> srcs;

    for (auto phi : phis) {
      auto src = assignment.at(phi->incomingFrom(pred));
      auto dst = assignment.at(phi->ret());
      // std::cerr << "src = " << phi->incomingFrom(pred) << ": " << src << "\n";
      // std::cerr << "dst = " << phi->ret() << ": " << dst << "\n";
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
      emitCopy(copy[start].first, scratch[0], ty);
      for (int i = (int) cycle.size() - 1; i > 0; --i) {
        auto [dst, ty] = cycle[i];
        emitCopy(copy[dst].first, dst, ty);
      }
      emitCopy(scratch[0], start, ty);

      // Delete the cycle from the registers.
      for (auto r : cycle)
        copy.erase(r.first);
    }
  }
}

}
