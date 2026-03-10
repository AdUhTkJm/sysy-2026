#include "../opt/Pass.h"
#include <unordered_set>
#include <stack>

namespace data {

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

  auto &getSccs() { return sccs; }
  const auto &getSccs() const { return sccs; }

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