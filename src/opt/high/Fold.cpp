#include "Common.h"
#include "../../ir/Matcher.h"

using namespace match;

namespace opt {

#define when & [](const Env &env) -> bool
#define imm(x) env.imms.at(x)

static Rule rules[] = {
  Rule("(addi x (int 'a))") >> "x" when { return imm("'a") == 0; },
  Rule("(addi (int 'a) (int 'b))") >> "(int:i32 (!add 'a 'b))",
  Rule("(addi (subi (int 'a) x) (int 'b))") >> "(subi:i32 (!add 'a 'b) x)",

  Rule("(addl x (int 'a))") >> "x" when { return imm("'a") == 0; },

  Rule("(subi (int 'a) (int 'b))") >> "(int:i32 (!sub 'a 'b))",
  Rule("(subi (int 'a) (subi x y))") >> "(subi:i32 y x)" when { return imm("'a") == 0; },
  Rule("(subi (subi (int 'a) x) (int 'b))") >> "(subi:i32 (!sub 'a 'b) x)",

  Rule("(sext (int 'a))") >> "(int:i32 'a)",
  
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

    // Remove inner if's with the same condition.
    walk<Postorder>(op, [&](Op *br) {
      if (op == br || !isa<IfOp>(br))
        return;

      if (cond != br->val())
        return;

      auto preserved = br->getRegion(0);
      auto yield = preserved->getLastOp();
      for (auto bb : *preserved)
        bb->inlineBefore(br);
      
      for (auto [i, v] : data::enumerate(br->getResults()))
        v->replaceAllUsesWith(yield->val(i));
      
      br->erase();
      yield->erase();
    });

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