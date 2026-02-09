#include "Common.h"
#include "../../ir/Matcher.h"

using namespace match;

namespace opt {

Pass *makeHighDCE(ir::ModuleOp *module);

Rule rules[] = {
  Rule("(addw x (movi 'a))") >> "(addwi:i32 x 'a)",
  Rule("(str (addxi base 'a) val 'b)") >> "(str base val (!add 'a 'b))",
  Rule("(ldr:T (addxi base 'a) 'b)") >> "(ldr:T base (!add 'a 'b))",
};

declare_pass(InstCombine) {
  walk<Postorder>(module, [](Op *op) {
    for (auto &rule : rules) {
      if (rule.rewrite(op))
        break;
    }
  });
}

}
