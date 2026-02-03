#include "Common.h"
#include <algorithm>

namespace opt {

declare_pass(HighDCE) {
  fixed(
    walk<Postorder>(module, [&](Op *op) {
      if (std::all_of(op->getResults().begin(), op->getResults().end(), [](Value *v){
        return !v->used();
      }) && !hasSideEffect(op)) {
        std::cout << "erasing " << op << "\n";
        op->erase();
        mark_changed;
      }
    });
  );
}

}
