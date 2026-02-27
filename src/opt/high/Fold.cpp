#include "Common.h"
#include "../../ir/Matcher.h"

using namespace match;

namespace opt {

static Rule rules[] = {
  Rule("(addi (int 'a) (int 'b))") >> "(int:i32 (!add 'a 'b))",
  Rule("(subi (int 'a) (int 'b))") >> "(int:i32 (!sub 'a 'b))",
  Rule("(muli (int 'a) (int 'b))") >> "(int:i32 (!mul 'a 'b))",
  Rule("(divi (int 'a) (int 'b))") >> "(int:i32 (!div 'a 'b))",
  Rule("(modi (int 'a) (int 'b))") >> "(int:i32 (!mod 'a 'b))",
  Rule("(lt (int 'a) (int 'b))") >> "(int:i32 (!lt 'a 'b))",
  Rule("(le (int 'a) (int 'b))") >> "(int:i32 (!le 'a 'b))",
  Rule("(eq (int 'a) (int 'b))") >> "(int:i32 (!eq 'a 'b))",
  Rule("(ne (int 'a) (int 'b))") >> "(int:i32 (!ne 'a 'b))",
};

declare_local_pass(Fold,
  bool rewrite(Op *op);
) {
  fixed(walk<Postorder>(func, [&](Op *op) {
    for (auto &rule : rules) {
      if (rule.rewrite(op)) {
        mark_changed;
        return;
      }
    }

    if (rewrite(op)) {
      mark_changed;
      return;
    }
  });)
}

bool Fold::rewrite(Op *op) {
  if (isa<IfOp>(op)) {
    auto cond = op->val();
    auto i = dyn_cast<IntOp>(cond->def);
    if (!i)
      return false;

    auto preserved = op->getRegion(i->value == 0);
    auto yield = preserved->getLastOp();
    for (auto bb : *preserved)
      bb->inlineBefore(op);
    
    for (auto [i, v] : data::enumerate(op->getResults()))
      v->replaceAllUsesWith(yield->val(i));
    
    op->erase();
    yield->erase();
    return true;
  }

  return false;
}

}