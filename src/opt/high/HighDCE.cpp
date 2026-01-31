#include "Common.h"
#include <algorithm>

namespace opt {

declare_pass(HighDCE) {
  fixed(
    std::vector<Op*> remove;
    walk(module, [&](Op *op) {
      if (std::all_of(op->getResults().begin(), op->getResults().end(), [](Value *v){
        return !v->used();
      }) && isPure(op))
        remove.push_back(op);
    });
    for (auto x : remove)
      x->erase();
    if (remove.size() > 0)
      mark_changed;
    remove.clear();
  );
}

}
