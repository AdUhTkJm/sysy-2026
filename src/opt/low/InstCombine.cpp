#include "Common.h"
#include "../../ir/Matcher.h"
#include <cmath>

using namespace match;

namespace opt {

Pass *makeHighDCE(ir::ModuleOp *module);

#define when & [](const Env &env) -> bool
#define imm(x) env.imms.at(x)

static Rule rules[] = {
  Rule("(addw (movi 'a) (movi 'b))") >> "(movi:i32 (!add 'a 'b))",
  Rule("(addw x (movi 'a))") >> "(addwi:i32 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },

  Rule("(addx x (movi 'a))") >> "(addxi:i64 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },
  Rule("(addx x (movl 'a))") >> "(addxi:i64 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },

  Rule("(subw (movi 'a) (movi 'b))") >> "(movi:i32 (!sub 'a 'b))",
  Rule("(subw x (movi 'a))") >> "(subwi:i32 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },

  Rule("(subx x (movi 'a))") >> "(subxi:i64 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },
  Rule("(subx x (movl 'a))") >> "(subxi:i64 x 'a)" when { return imm("'a") < 4096 && imm("'a") >= 0; },

  Rule("(mulw x (movi 'a))") >> "x" when { return imm("'a") == 1; },
  Rule("(mulw (movi 'a) (movi 'b))") >> "(movi:i32 (!mul 'a 'b))",

  Rule("(lslw x (movi 'a))") >> "(lslwi:i32 x 'a)",
  Rule("(lsrw x (movi 'a))") >> "(lsrwi:i32 x 'a)",

  Rule("(addwi x 'a)") >> "x" when { return imm("'a") == 0; },
  Rule("(addxi x 'a)") >> "x" when { return imm("'a") == 0; },

  Rule("(str (addxi base 'a) val 'b)") >> "(str base val (!add 'a 'b))",
  Rule("(ldr:T (addxi base 'a) 'b)") >> "(ldr:T base (!add 'a 'b))",
};

declare_local_pass(InstCombine,
  void combineBranch(Op *op) const;
  bool rewrite(Op *op) const;

  bool rewriteMul(Op *op, Value *v, int mul) const;
  bool rewriteDiv(Op *op, Value *v, int div) const;
  bool rewriteMod(Op *op, int mod) const;
) {
  // We first rewrite all mods, otherwise it's very hard to reconstruct the pattern after divs are lowered.
  walk<Postorder>(func, [&](Op *op) {
    if (isa<MsubWOp>(op)) {
      auto y = op->val(1);
      // x % y is lowered as (msubw (sdivw x y) y x). Match it.
      if (auto mov = dyn_cast<MovIOp>(y->def))
        rewriteMod(op, mov->value);
    }
  });

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
    replace(FcmpEqOp, BeqFOp);
    replace(FcmpNeOp, BneFOp);
    replace(FcmpLtOp, BltFOp);
    replace(FcmpLeOp, BleFOp);
    return;
  }

  if (auto br = dyn_cast<CbzOp>(op)) {
    replace(CmpEqOp, BneOp);
    replace(CmpNeOp, BeqOp);
    replace(CmpLtOp, BgeOp);
    replace(CmpLeOp, BgtOp);
    replace(FcmpEqOp, BneFOp);
    replace(FcmpNeOp, BeqFOp);
    replace(FcmpLtOp, BgeFOp);
    replace(FcmpLeOp, BgtFOp);
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

  if (isa<DivWOp>(op)) {
    auto lhs = op->val(0), rhs = op->val(1);
    if (auto mov = dyn_cast<MovIOp>(rhs->def))
      return rewriteDiv(op, lhs, mov->value);

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
    if (auto lsl = dyn_cast<LslWIOp>(rhs->def); lsl && lsl->value <= 4) {
      auto addw = builder.replace<AddSxtOp>(op, i64)->with(lhs, lsl->val());
      addw->value = lsl->value;
      return true;
    }
    if (auto lsl = dyn_cast<LslWIOp>(lhs->def); lsl && lsl->value <= 4) {
      auto addw = builder.replace<AddSxtOp>(op, i64)->with(rhs, lsl->val());
      addw->value = lsl->value;
      return true;
    }
    return false;
  }

  if (auto str = dyn_cast<StrOp>(op)) {
    Builder builder;
    auto base = op->val(0), val = op->val(1);
    int imm = str->value;
    if (auto add = dyn_cast<AddXIOp>(base->def); add) {
      auto s = builder.replace<StrOp>(op)->with(add->val(), val);
      s->value = imm + add->value;
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

struct Multiplier {
  int shPost;
  unsigned long mHigh;
  int l;
};

[[gnu::unused]] static Multiplier chooseMultiplier(int d) {
  constexpr int N = 32;
  // Number of bits of precision needed. Note we only need 31 bits,
  // because there's a sign bit.
  constexpr int prec = N - 1;
  
  int l = std::ceil(std::log2((double) d));
  int shPost = l;
  unsigned long mLow = (1ull << (N + l)) / d;
  unsigned long mHigh = ((1ull << (N + l)) + (1ull << (N + l - prec))) / d;
  while (mLow / 2 < mHigh / 2 && shPost > 0) {
    mLow /= 2;
    mHigh /= 2;
    shPost--;
  }
  return { shPost, mHigh, l };
}

bool InstCombine::rewriteDiv(Op *op, Value *v, int div) const {
  if (div == 1) {
    op->ret()->replaceAllUsesWith(v);
    op->erase();
    return true;
  }

  if (div <= 0)
    return false;

  Builder builder;
  if (div == 2) {
    builder.setBefore(op);

    // add     w8, w0, w0, lsr #31
    // asr     w0, w8, #1
    auto add = builder.create<AddWLsrOp>(i32)->with(v, v);
    add->value = 31;
    auto asr = builder.replace<AsrWIOp>(op, i32)->with(add->ret());
    asr->value = 1;
    return true;
  }

  if (__builtin_popcount(div) == 1) {
    builder.setBefore(op);

    // add     w8, w0, #(2^n - 1)
    // cmp     w0, #0
    // csel    w8, w8, w0, lt
    // asr     w0, w8, #n
    auto vi = builder.create<MovIOp>(i32);
    vi->value = div - 1;
    
    auto add = builder.create<AddWOp>(i32)->with(v, vi->ret());
    auto csel = builder.create<CselLtIOp>(i32)->with(v, add->ret(), v);
    csel->value = 0;
    
    auto asr = builder.replace<AsrWIOp>(op, i32)->with(csel->ret());
    asr->value = __builtin_ctz(div);
    return true;
  }

  auto [shPost, m, l] = chooseMultiplier(div);
  builder.setBefore(op);
  if (m < (1ull << 31)) {
    auto mVal = builder.create<MovIOp>(i32); mVal->value = m;
    auto mulsh = builder.create<SmullOp>(i64)->with(v, mVal->ret());
    auto sra = builder.create<AsrXIOp>(i64)->with(mulsh->ret()); sra->value = 32 + shPost;
    auto cast = builder.create<CastOp>(i32)->with(sra->ret());
    auto add = builder.replace<AddWLsrOp>(op, i32)->with(cast->ret(), cast->ret()); add->value = 31;
    return true;
  } else {
    auto mVal = builder.create<MovIOp>(i32); mVal->value = m - (1ull << 32);
    auto mul = builder.create<SmullOp>(i64)->with(mVal->ret(), v);
    auto mulsh = builder.create<AsrXIOp>(i64)->with(mul->ret()); mulsh->value = 32;
    auto cast = builder.create<CastOp>(i32)->with(mulsh->ret());
    auto add = builder.create<AddWOp>(i32)->with(cast->ret(), v);
    Op *sra = add;
    if (shPost > 0) {
      auto asr = builder.create<AsrWIOp>(i32)->with(add->ret());
      asr->value = shPost;
      sra = asr;
    }

    auto xsign = builder.create<AsrWIOp>(i32)->with(v); xsign->value = 31;
    builder.replace<SubWOp>(op, i32)->with(sra->ret(), xsign->ret());
    return true;
  }

  return false;
}

bool InstCombine::rewriteMod(Op *op, int mod) const {
  auto z = op->val(0);
  auto y = op->val(1);
  auto x = op->val(2);
  
  if (!isa<DivWOp>(z->def) || z->def->val(0) != x || z->def->val(1) != y || mod < 0)
    return false;

  Builder builder;
  if (mod == 2) {
    builder.setBefore(op);

    // and     w8, w0, #1
    // cmp     w0, #0
    // cneg    w0, w8, lt
    auto _and = builder.create<AndWIOp>(i32)->with(x); _and->value = 1;
    auto cneg = builder.replace<CnegLtIOp>(op, i32)->with(x, _and->ret()); cneg->value = 0;
    return true;
  }

  if (__builtin_popcount(mod) == 1) {
    builder.setBefore(op);

    // negs    w8, w0
    // and     w9, w0, #(mod - 1)
    // and     w8, w8, #(mod - 1)
    // csneg   w0, w9, w8, mi
    auto negs = builder.create<NegsOp>(i32)->with(x);
    auto _and1 = builder.create<AndWIOp>(i32)->with(x); _and1->value = mod - 1;
    auto _and2 = builder.create<AndWIOp>(i32)->with(negs->ret()); _and2->value = mod - 1;
    builder.replace<CsnegMiOp>(op, i32)->with(_and1->ret(), _and2->ret());
    return true;
  }

  return false;
}

}
