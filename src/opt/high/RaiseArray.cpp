#include "Common.h"
#include <unordered_set>
#include <stack>

namespace {
  
// The Tarjan algorithm of finding 
class Tarjan {
public:
  Tarjan(const opt::Pass::CallGraph &cg): graph(cg) {
    // Run Tarjan.
    for (auto &[v, _] : graph) {
      if (!index.count(v))
        strongConnect(v);
    }
  }

  std::set<ir::FuncOp*> getSccContaining(ir::FuncOp* target) {
    // Find the SCC that contains target.
    for (const auto &scc : sccs) {
      if (scc.count(target))
        return scc;
    }
    
    return {};
  }

private:
  const opt::Pass::CallGraph &graph;
  // We visit the nodes in DFS order, and assign an increasing index to them when we visit.
  std::unordered_map<ir::FuncOp*, int> index;
  // This is the lowest link achievable on DFS.
  std::unordered_map<ir::FuncOp*, int> lowlink;
  // This is the DFS stack, explicitly managed.
  std::stack<ir::FuncOp*> s;
  // This is the same as `s`, but converted to a set, for better checking of inclusion.
  std::unordered_set<ir::FuncOp*> onStack;
  // Results.
  std::vector<std::set<ir::FuncOp*>> sccs;
  int cnt = 0;

  void strongConnect(ir::FuncOp* v) {
    index[v] = cnt;
    lowlink[v] = cnt;
    cnt++;
    s.push(v);
    onStack.insert(v);

    // Consider successors of v.
    if (auto it = graph.find(v); it != graph.end()) {
      for (ir::FuncOp *w : it->second) {
        if (!index.count(w)) {
          // Successor w has not yet been visited; recurse.
          strongConnect(w);
          lowlink[v] = std::min(lowlink[v], lowlink[w]);
        } else if (onStack.count(w)) {
          // Successor w is in stack. Update lowlink.
          lowlink[v] = std::min(lowlink[v], index[w]);
        }
      }
    }

    // If v is a root node, pop the stack and generate an SCC.
    if (lowlink[v] == index[v]) {
      std::set<ir::FuncOp*> scc;
      ir::FuncOp *w;
      do {
        w = s.top();
        s.pop();
        onStack.erase(w);
        scc.insert(w);
      } while (w != v);
      sccs.push_back(std::move(scc));
    }
  }
};

}

namespace opt {

declare_pass(RaiseArray,
  void raiseRecursive(FuncOp *func);
  void runImpl(FuncOp *func);

  Tarjan *tarjan;
) {
  auto cg = callGraph();
  Tarjan t(cg);
  tarjan = &t;

  for (auto fn : collectFunctions())
    runImpl(fn);
}

void RaiseArray::runImpl(FuncOp *func) {
  if (func->has<RecursiveAttr>()) {
    raiseRecursive(func);
    return;
  }

  Builder builder;
  int i = 0;
  for_all(AllocaOp, func) {
    auto dim = op->get<DimAttr>();
    if (!dim)
      continue;

    auto ty = op->ret()->type->pointee();
    builder.setToStart(module->getRegion());
    auto global = builder.create<GlobalOp>(ty);
    global->set<DimAttr>(*dim);
    global->name = "__arr_" + func->name + "_" + std::to_string(i++);

    if (ty == i32)
      global->set<ConstIArrAttr>(std::vector<int>(dim->size()));
    else {
      assert(ty == f32);
      global->set<ConstFArrAttr>(std::vector<float>(dim->size()));
    }

    builder.replace<GetGlobalOp>(op, Type::pointer(ty))->with(global->ret());
  }
}

// The basic idea is to emulate a stack of 1MB with arrays.
void RaiseArray::raiseRecursive(FuncOp *func) {
  Builder builder;
  int i = 0;
  auto allocas = collectOps<AllocaOp>();
  if (allocas.empty())
    return;

  // We need to add a `depth` argument to the function.
  // This is used to index the emulated stack.

  // For all calls to `func` inside the same recursive cycle, we need to increase depth by 1.
  // All of them now need a new depth argument.
  // For calls between them, the depth argument is passed as-is.
  std::set<FuncOp*> scc = tarjan->getSccContaining(func);
  std::map<FuncOp*, Value*> depths;
  for (auto fn : scc)
    depths[fn] = fn->pushResult(i32);

  for (auto fn : scc) {
    for_all(CallOp, fn) {
      if (!scc.count(cast<FuncOp>(op->val(0)->def)))
        continue;

      op->pushOperand(depths[fn]);
    }
  }
  
  auto depth = depths[func];

  // For all other functions, start with a depth of 0 when it calls any of these functions.
  for (auto fn : collectFunctions()) {
    if (scc.count(fn))
      continue;

    for_all(CallOp, fn) {
      if (!scc.count(cast<FuncOp>(op->val(0)->def)))
        continue;

      builder.setBefore(op);
      auto zero = builder.create<IntOp>(i32);
      op->pushOperand(zero->ret());
    }
  }

  for (auto op : allocas) {
    auto dim = op->get<DimAttr>();
    if (!dim)
      continue;

    // Emulate an 1MB stack with a global array.
    auto size = dim->size();
    auto firstDim = (1048576 + size - 1) / size;
    std::vector<int> dims { firstDim };
    data::concat(dims, dim->dims);
    size *= firstDim;

    auto ty = op->ret()->type->pointee();
    builder.setToStart(module->getRegion());
    auto global = builder.create<GlobalOp>(ty);
    global->set<DimAttr>(dims);
    global->name = "__arr_" + func->name + "_" + std::to_string(i++);

    if (ty == i32)
      global->set<ConstIArrAttr>(std::vector<int>(size));
    else {
      assert(ty == f32);
      global->set<ConstFArrAttr>(std::vector<float>(size));
    }

    builder.setBefore(op);
    auto get = builder.replace<GetGlobalOp>(op, Type::pointer(ty))->with(global->ret());

    // All references to the alloca should be replaced with `%get[%depth]`.
    for (auto use : get->ret()->getUses()) {
      assert(isa<ArrayLoadOp>(use) || isa<ArrayStoreOp>(use));
      use->insertOperand(1, depth);
    }
  }
}

}
