#include "Common.h"
#include "../../ir/Matcher.h"

using namespace match;

namespace opt {

Rule rules[] = {
  Rule("(addw x (movi 'a))") >> "(addwi:i64 x 'a)",
  Rule("(str val (addx base (movi 'a)) 'b)") >> "(str val base (!add 'a 'b))",
};

declare_pass(InstCombine) {
  walk(module, [](Op *op) {
    for (auto &rule : rules)
      rule.rewrite(op);
  });
}

}
