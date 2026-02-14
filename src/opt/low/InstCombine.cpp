#include "Common.h"
#include "../../ir/Matcher.h"

using namespace match;

namespace opt {

Pass *makeHighDCE(ir::ModuleOp *module);

#define pred [](const Env &env) -> bool
#define imm(x) env.imms.at(x)

static Rule rules[] = {
  Rule("(addw x (movi 'a))") >> "(addwi:i32 x 'a)",
  Rule("(addx x (movi 'a))") >> "(addxi:i32 x 'a)",
  Rule("(mulw x (movi 'a))") >> "x" & pred { return imm("'a") == 1; },
  Rule("(mulw (movi 'a) (movi 'b))") >> "(movi:i32 (!mul 'a 'b))",
  Rule("(addwi x 'a)") >> "x" & pred { return imm("'a") == 0; },
  Rule("(addxi x 'a)") >> "x" & pred { return imm("'a") == 0; },
  Rule("(str (addxi base 'a) val 'b)") >> "(str base val (!add 'a 'b))",
  Rule("(ldr:T (addxi base 'a) 'b)") >> "(ldr:T base (!add 'a 'b))",
};

declare_local_pass(InstCombine,
  void combineBranch(Op *op);
) {
  fixed(walk<Postorder>(func, [&](Op *op) {
    for (auto &rule : rules) {
      if (rule.rewrite(op)) {
        mark_changed;
        break;
      }
    }
  });)

  for (auto bb : *func->getRegion())
    combineBranch(bb->getLastOp());
}

#define replace(Before, After) \
  if (auto cond = dyn_cast<Before>(br->val()->def)) { \
    auto target = br->target; \
    auto other = br->other; \
    builder.setBefore(op); \
    auto renamed = builder.create<After>()->with(cond->getOperands()); \
    renamed->target = target; \
    renamed->other = other; \
    op->erase(); \
    return; \
  }

void InstCombine::combineBranch(Op *op) {
  Builder builder;
  if (auto br = dyn_cast<CbnzOp>(op)) {
    replace(CmpEqOp, BeqOp);
    replace(CmpNeOp, BneOp);
    replace(CmpLtOp, BltOp);
    replace(CmpLeOp, BleOp);
    return;
  }

  if (auto br = dyn_cast<CbzOp>(op)) {
    replace(CmpEqOp, BneOp);
    replace(CmpNeOp, BeqOp);
    replace(CmpLtOp, BgeOp);
    replace(CmpLeOp, BltOp);
    return;
  }
}

}
