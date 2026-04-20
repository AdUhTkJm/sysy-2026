#include "Common.h"
#include "../../ir/Matcher.h"

using namespace match;

namespace opt {

#define when & [](const Env &env) -> bool
#define zero { return imm("'a") == 0; }
#define imm(x) env.imms.at(x)

static Rule rules[] = {
  Rule("(addi x (int 'a))") >> "x" when zero,
  Rule("(addi (int 'a) (int 'b))") >> "(int:i32 (!add 'a 'b))",
  Rule("(addi (int 'a) x)") >> "(addi:i32 x (int:i32 'a))",
  Rule("(addi (subi (int 'a) x) (int 'b))") >> "(subi:i32 (int:i32 (!add 'a 'b)) x)",
  Rule("(addi (addi x (int 'a)) (int 'b))") >> "(addi:i32 x (int:i32 (!add 'a 'b)))",
  Rule("(addi (addi x (int 'a)) y)") >> "(addi:i32 (addi:i32 x y) (int:i32 'a))",
  Rule("(addi (addi x y) (subi z y))") >> "(addi:i32 x z)",
  Rule("(addi x x)") >> "(muli:i32 x (int:i32 2))",
  Rule("(addi (addi x (int 'a)) y)") >> "(addi:i32 (addi:i32 x y) (int:i32 'a))",
  Rule("(addi (subi (int 'a) x) y)") >> "(subi:i32 y x)" when zero,
  Rule("(addi (subi x y) y)") >> "x",

  Rule("(addl x (int64 'a))") >> "x" when zero,
  Rule("(addl (int64 'a) (int64 'b))") >> "(int64:i64 (!add 'a 'b))",
  Rule("(addl (addl x (int64 'a)) (int64 'b))") >> "(addl:i64 x (int64:i64 (!add 'a 'b)))",
  Rule("(addl (addl x (int64 'a)) y)") >> "(addl:i64 (addl:i64 x y) (int64:i64 'a))",

  Rule("(subi (int 'a) (int 'b))") >> "(int:i32 (!sub 'a 'b))",
  Rule("(subi x x)") >> "(int:i32 0)",
  Rule("(subi x (int 'a))") >> "x" when zero,
  Rule("(subi (int 'a) (subi x y))") >> "(subi:i32 y x)" when zero,
  Rule("(subi (subi (int 'a) x) y)") >> "(subi:i32 (int:i32 'a) (addi:i32 x y))",
  Rule("(subi (subi x y) x)") >> "(subi:i32 (int:i32 0) y)",
  Rule("(subi (subi (int 'a) x) (int 'b))") >> "(subi:i32 (int:i32 (!sub 'a 'b)) x)",
  Rule("(subi (addi y x) (addi z x))") >> "(subi:i32 y z)",
  Rule("(subi (addi x y) (addi x z))") >> "(subi:i32 y z)",
  Rule("(subi x (addi x y))") >> "(subi:i32 (int:i32 0) y)",
  Rule("(subi x (addi y x))") >> "(subi:i32 (int:i32 0) y)",
  Rule("(subi (addi x y) x)") >> "y",
  Rule("(subi (addi x y) y)") >> "x",
  Rule("(subi (addi (subi x z) y) (addi x y))") >> "(subi:i32 (int:i32 0) z)",

  Rule("(subl (int64 'a) (int64 'b))") >> "(int64:i64 (!sub 'a 'b))",
  Rule("(subl x (int64 'a))") >> "x" when zero,
  Rule("(subl (int64 'a) (subl x y))") >> "(subl:i64 y x)" when zero,
  Rule("(subl (subl (int64 'a) x) (int64 'b))") >> "(subl:i64 (!sub 'a 'b) x)",
  Rule("(subl (addl x y) x)") >> "y",
  Rule("(subl (addl y x) x)") >> "y",
  Rule("(subl x (int64 'a))") >> "x" when zero,

  Rule("(sext (int 'a))") >> "(int64:i64 'a)",
  
  Rule("(muli (int 'a) (int 'b))") >> "(int:i32 (!mul 'a 'b))",
  Rule("(muli x (int 'a))") >> "x" when { return imm("'a") == 1; },

  Rule("(divi (int 'a) (int 'b))") >> "(int:i32 (!div 'a 'b))",
  Rule("(modi (int 'a) (int 'b))") >> "(int:i32 (!mod 'a 'b))",
  
  Rule("(lt (int 'a) (int 'b))") >> "(int:i32 (!lt 'a 'b))",
  Rule("(lt x (subi y z))") >> "(lt:i32 (addi:i32 x z) y)",
  Rule("(lt (subi (int 'a) x) (int 'b))") >> "(lt:i32 (int:i32 (!sub 'a 'b)) x)",
  Rule("(lt (muli x (int 'a)) (int 'b))") >> "(lt:i32 x (int:i32 (!div 'b 'a)))" when {
    return imm("'a") > 0 && imm("'b") > 0 && imm("'b") % imm("'a") == 0;
  },
  Rule("(lt (muli x (int 'a)) (int 'b))") >> "(lt:i32 x (int:i32 (!add (!div 'b 'a) 1)))" when {
    return imm("'a") > 0 && imm("'b") > 0 && imm("'b") % imm("'a") != 0;
  },

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