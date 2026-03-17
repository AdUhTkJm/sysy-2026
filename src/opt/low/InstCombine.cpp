#include "Common.h"
#include "../../ir/Matcher.h"

using namespace match;

namespace opt {

Pass *makeHighDCE(ir::ModuleOp *module);

#define when & [](const Env &env) -> bool
#define imm(x) env.imms.at(x)

static Rule rules[] = {
  Rule("(addw (movi 'a) (movi 'b))") >> "(movi:i32 (!add 'a 'b))",
  Rule("(addw x (movi 'a))") >> "(addwi:i32 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },
  Rule("(addx x (movi 'a))") >> "(addxi:i64 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },
  Rule("(subw (movi 'a) (movi 'b))") >> "(movi:i32 (!sub 'a 'b))",
  Rule("(subw x (movi 'a))") >> "(subwi:i32 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },
  Rule("(subx x (movi 'a))") >> "(subxi:i64 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },
  Rule("(mulw x (movi 'a))") >> "x" when { return imm("'a") == 1; },
  Rule("(mulw (movi 'a) (movi 'b))") >> "(movi:i32 (!mul 'a 'b))",
  Rule("(addwi x 'a)") >> "x" when { return imm("'a") == 0; },
  Rule("(addxi x 'a)") >> "x" when { return imm("'a") == 0; },
  Rule("(str (addxi base 'a) val 'b)") >> "(str base val (!add 'a 'b))",
  Rule("(ldr:T (addxi base 'a) 'b)") >> "(ldr:T base (!add 'a 'b))",
};

declare_local_pass(InstCombine,
  void combineBranch(Op *op) const;
  bool rewrite(Op *op) const;

  bool rewriteMul(Op *op, Value *v, int mul) const;
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

void InstCombine::combineBranch(Op *op) const {
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

// Here we rewrite operations that can hardly be matched by any rule.
bool InstCombine::rewrite(Op *op) const {
  if (isa<MulWOp>(op)) {
    auto lhs = op->val(0), rhs = op->val(1);
    if (auto mov = dyn_cast<MovIOp>(rhs->def))
      return rewriteMul(op, lhs, mov->value);
    else if (auto mov = dyn_cast<MovIOp>(lhs->def))
      return rewriteMul(op, rhs, mov->value);

    return false;
  }

  if (isa<AddWOp>(op)) {
    Builder builder;
    auto lhs = op->val(0), rhs = op->val(1);
    
    //   lsl %rhs, %1, #x
    //   add %out, %lhs, %rhs
    // =>
    //   add %out, %lhs, %1, #x
    if (auto lsl = dyn_cast<LslWIOp>(rhs->def)) {
      auto addw = builder.replace<AddWLslOp>(op, i32)->with(lhs, lsl->val());
      addw->value = lsl->value;
      return true;
    }
    if (auto lsl = dyn_cast<LslWIOp>(lhs->def)) {
      auto addw = builder.replace<AddWLslOp>(op, i32)->with(rhs, lsl->val());
      addw->value = lsl->value;
      return true;
    }
    return false;
  }

  if (isa<AddXOp>(op)) {
    Builder builder;
    auto lhs = op->val(0), rhs = op->val(1);
    
    //   lsl %rhs, %1, #x
    //   add %out, %lhs, %rhs
    // =>
    //   add %out, %lhs, %1, #x
    if (auto lsl = dyn_cast<LslWIOp>(rhs->def)) {
      auto addw = builder.replace<AddXLslOp>(op, i64)->with(lhs, lsl->val());
      addw->value = lsl->value;
      return true;
    }
    if (auto lsl = dyn_cast<LslWIOp>(lhs->def)) {
      auto addw = builder.replace<AddXLslOp>(op, i64)->with(rhs, lsl->val());
      addw->value = lsl->value;
      return true;
    }
    return false;
  }

  return false;
}

bool InstCombine::rewriteMul(Op *op, Value *v, int mul) const {
  Builder builder;
  auto ty = op->ret()->type;

  if (mul == 0) {
    auto movi = builder.replace<MovIOp>(op, ty);
    movi->value = 0;
    return true;
  }

  int popcount = __builtin_popcount(mul);

  // This can be expressed with a single left shift.
  if (popcount == 1) {
    int ctz = __builtin_ctz(mul);
    auto lsl = builder.replace<LslWIOp>(op, ty)->with(v);
    lsl->value = ctz;
    return true;
  }

  if (popcount == 2) {
    // This is:
    //   add %0, v, v, lsl #(ctz1 - ctz0);
    //   lsl %1, %0, #ctz0
    int ctz0 = __builtin_ctz(mul);
    int ctz1 = __builtin_ctz(mul - (1 << ctz0));

    builder.setBefore(op);
    auto add = builder.create<AddWLslOp>(ty)->with(v, v);
    add->value = ctz1 - ctz0;

    if (ctz0 != 0) {
      auto lsl = builder.replace<LslWIOp>(op, ty)->with(add->ret());
      lsl->value = ctz0;
    } else {
      op->ret()->replaceAllUsesWith(add->ret());
      op->erase();
    }
    return true;
  }

  return false;
}

}
