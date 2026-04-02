#include "Common.h"
#include "../../utils/presburger/Expr.h"
#include <algorithm>
#include <optional>

using namespace pres;

namespace opt {

declare_pass(SCEV,
  void evolve(DoWhileOp *loop);
  Value *generate(Value *ind, const Expr &delta);
  Value *generateStart(Value *target, Op *anchor);
  void moveChainBefore(Op *op, Op *anchor);

  Builder builder;
  std::set<DoWhileOp*> visited;
  std::set<Value*> variants;
  std::unordered_map<Value *, Value *> starts;
  DoWhileOp *loop;

  std::vector<Op*> generated;
) {
  for_all(DoWhileOp)
    evolve(op);
}

#define creation_list(X) \
  X(Add) X(Sub) X(Mul) X(Div)

#define creation_impl(ty, v0, v1) \
  k->type == Type::ty ? builder.create<ty##IOp>(i32)->with(v0, v1)->ret() :

#define creation_impl_1(ty) \
  creation_impl(ty, sym.at(k->ops[0]), sym.at(k->ops[1]))

#define creation_impl_2(ty) \
  creation_impl(ty, v, sym.at(k->ops[i]))

bool nonHoistable(Op *op);

Value *SCEV::generate(Value *ind, const Expr &delta) {
  using Type = Expr::Type;

  std::unordered_map<const void *, Value *> sym;
  bool failed = false;

  delta.walk([&](const Expr::ExprImpl *k) {
    if (failed)
      return;

    switch (k->type) {
    case Type::ConstInt:
      sym[k] = builder.createInt(k->vi)->ret();
      generated.push_back(sym[k]->def);
      break;
    case Type::Parameter:
      // We must make sure that the value is directly under the while,
      // i.e. not nested in any if-statement.
      if (k->v->def->getParentOp() != loop)
        failed = true;
      
      sym[k] = k->v;
      break;
    case Type::Indvar:
      sym[k] = ind;
      break;
    default:
      Value *v = creation_list(creation_impl_1) nullptr;
      generated.push_back(v->def);
      for (unsigned i = 2; i < k->nops; i++) {
        v = creation_list(creation_impl_2) nullptr;
        generated.push_back(v->def);
      }
      sym[k] = v;
      break;
    }
  });

  if (failed) {
    for (auto x : generated)
      x->clearOperands();
    for (auto x : generated)
      x->erase();

    generated.clear();
    return nullptr;
  }

  generated.clear();
  return sym.at(delta.impl);
}

void SCEV::moveChainBefore(Op *op, Op *anchor) {
  if (!op->inside(loop))
    return;

  op->moveBefore(anchor);
  for (auto x : op->getOperands())
    moveChainBefore(x->def, op);
}

Value *SCEV::generateStart(Value *target, Op *anchor) {
  if (auto it = starts.find(target); it != starts.end())
    return it->second;
  if (!variants.count(target)) {
    moveChainBefore(target->def, anchor);
    return target;
  }

  Builder::Guard _(builder);

  auto cloned = builder.clone(target->def);
  for (unsigned i = 0; i < cloned->getNumOperands(); i++) {
    builder.setBefore(cloned);
    cloned->setOperand(i, generateStart(cloned->val(i), cloned));
  }
  
  return starts[target] = cloned->ret(target->def->getResultIndex(target));
}

void SCEV::evolve(DoWhileOp *loop) {
  if (visited.count(loop))
    return;
  visited.insert(loop);

  for_all(DoWhileOp, loop)
    evolve(op);

  // First identify the induction variables.
  this->loop = loop;
  auto region = loop->getRegion();
  auto cond = cast<ConditionOp>(region->getLastOp());

  // The presburger expression for each value.
  std::map<Value*, Expr> pres;
  // The values on which we do not do strength reduction.
  std::set<Value*> dont;
  variants = getVariantsIn(loop);
  Value *ind = nullptr, *inc; int index;

  for (unsigned i = 1; i < cond->getNumOperands(); i++) {
    auto v = cond->val(i);
    auto def = v->def;
    auto ret = loop->ret(i - 1);
    // Find `i = i + n`.
    if (isa<AddIOp>(def) && (def->val(0) == ret || def->val(1) == ret)) {
      auto other = def->val(def->val(0) == ret);
      if (!variants.count(other)) {
        pres[ind = ret] = Expr();
        inc = other;
        // The `i + n` part shouldn't be done with SCEV.
        dont.insert(v);
        index = i;
        break;
      }
    }
  }

  if (!ind)
    return;

  // When the loop stride is 1, we can find other auxiliary variables.
  if (auto vi = dyn_cast<IntOp>(inc->def)) {
    for (unsigned i = index + 1; i < cond->getNumOperands(); i++) {
      auto v = cond->val(i);
      auto def = v->def;
      auto ret = loop->ret(i - 1);
      if (isa<AddIOp>(def) && (def->val(0) == ret || def->val(1) == ret)) {
        auto other = def->val(def->val(0) == ret);
        if (variants.count(other))
          continue;

        auto vj = dyn_cast<IntOp>(other->def);
        if (vi->value == 1) {
          pres[ret] = Expr(other) * (Expr() - loop->val(index)) + loop->val(i - 1);
          dont.insert(v);
        } else if (vj->value % vi->value == 0) {
          pres[ret] = Expr(vj->value / vi->value) * (Expr() - loop->val(index)) + loop->val(i - 1);
          dont.insert(v);
        }
      }
    }
  }

  // Then find everything that relies on them.
  walk(loop, [&](Op *op) {
    // Sign extension does not change the op value.
    if (isa<SextOp>(op) && pres.count(op->val())) {
      pres[op->ret()] = pres[op->val()];
      return;
    }

    if (op->getNumOperands() != 2 || op->getNumResults() != 1)
      return;

    auto v = op->val(0), w = op->val(1);
    if (!pres.count(v)) {
      if (!pres.count(w))
        return;
      std::swap(v, w);
    }
    // Now `v` is definitely in `incr`, but we don't know whether `w` is in it.
    if (!pres.count(w)) {
      if (variants.count(w))
        return;
      // `w` is a constant (a parameter).
      pres[w] = Expr(w);
    }
    auto ev = pres.at(v), ew = pres.at(w);

    auto ret = op->ret();
    if (isa<AddIOp>(op) || isa<AddLOp>(op))
      pres[ret] = ev + ew;
    if (isa<SubIOp>(op))
      pres[ret] = ev - ew;
    if (isa<MulIOp>(op))
      pres[ret] = ev * ew;
    if (isa<DivIOp>(op))
      pres[ret] = ev / ew;
  });

  // Now generate addition for them.
  starts.clear();
  for (unsigned i = 0; i < loop->getNumResults(); i++)
    starts[loop->ret(i)] = loop->val(i);

  std::unordered_map<Value*, Value*> deferred;
  std::unordered_map<int, std::pair<Value*, Value*>> repr;
  for (const auto &[k, v] : pres) {
    if (dont.count(k) || loop->getResultIndex(k) != loop->getNumResults())
      continue;

    auto delta = (v.step(inc) - v).simplify();
    // This value increments by zero. No need to do anything to it.
    std::optional<int> recordVi;
    if (delta.impl->type == Expr::Type::ConstInt) {
      int vi = delta.impl->vi;
      if (vi == 0)
        continue;
    
      // The value `r` increases identically as `k`.
      // This means we don't need to introduce another induction variable;
      // we only need to compute their difference beforehand.
      if (auto it = repr.find(vi); it != repr.end()) {
        auto [r, rStart] = it->second;
        if (k->type != r->type)
          continue;

        builder.setBefore(loop);
        Value *start = generateStart(k, loop);
        Op *diff = k->type == i32
          ? (Op*) builder.create<SubIOp>(i32)->with(start, rStart)
          : (Op*) builder.create<SubLOp>(i64)->with(start, rStart);

        builder.setToStart(region);
        Op *add = k->type == i32
          ? (Op*) builder.create<AddIOp>(i32)->with(r, diff->ret())
          : (Op*) builder.create<AddLOp>(i64)->with(r, diff->ret());

        deferred[k] = add->ret();
        continue;
      }
      recordVi = vi;
    }

    builder.setBefore(k->def);
    // The increment carries something in the if-statement.
    Value *increment = generate(ind, delta);
    if (!increment)
      continue;

    // The result is not loop-carried. Now we make it so.
    // First generate a start value.
    builder.setBefore(loop);
    Value *start = generateStart(k, loop);
    Value *ret = loop->pushResult(k->type);
    loop->pushOperand(start);
    deferred[k] = ret;

    // Then update the condition's carried values.
    builder.setBefore(cond);
    Op *op = k->type == i32
      ? (Op*) builder.create<AddIOp>(i32)->with(ret, increment)
      : (Op*) builder.create<AddLOp>(i64)->with(ret, increment);
    cond->pushOperand(op->ret());

    if (recordVi)
      repr[*recordVi] = { ret, start };
  }

  for (auto [k, v] : deferred)
    k->replaceAllUsesWith(v);
}

}
