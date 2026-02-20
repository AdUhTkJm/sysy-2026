#include "Common.h"
#include <queue>

namespace opt {

declare_pass(Recursive) {
  std::map<Op*, std::vector<Op*>> callGraph;
  for_all(FuncOp) {
    auto func = op;
    for_all(CallOp, func)
      callGraph[func].push_back(op->val()->def);
  }

  // Toposort the graph.
  std::unordered_map<Op*, int> indegree;
  for (const auto &[fn, _] : callGraph)
    indegree[fn] = 0;

  for (const auto &[fn, vec] : callGraph) {
    for (auto op : vec)
      indegree[op]++;
  }

  std::queue<Op*> q;
  for (const auto &[fn, deg] : indegree) {
    if (deg == 0)
      q.push(fn);
  }

  while (!q.empty()) {
    Op *fn = q.front();
    q.pop();

    if (!callGraph.count(fn))
      continue;

    const auto &vec = callGraph[fn];
    for (auto op : vec) {
      if (callGraph.count(op)) {
        if (--indegree[op] == 0)
          q.push(op);
      }
    }
    callGraph.erase(fn);
  }
  // Now the remaining ones form cycles.
  for (const auto &[fn, _] : callGraph)
    fn->set<RecursiveAttr>();
}

}
