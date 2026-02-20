#include "Common.h"

namespace opt {

declare_pass(TidyCodeGen) {
  // Insert a call to __init(), at the top of main function.
  FuncOp *main = nullptr;
  FuncOp *init = nullptr;
  auto fns = collectFunctions();

  for (auto fn : fns) {
    if (fn->name == "main") {
      main = fn;
      break;
    }
  }
  for (auto fn : fns) {
    if (fn->name == "__init") {
      init = fn;
      break;
    }
  }

  assert(main && "main function must exist");
  Builder builder;
  builder.setToStart(main->getRegion());
  builder.create<CallOp>(unit)->with(init->ret());
}

}
