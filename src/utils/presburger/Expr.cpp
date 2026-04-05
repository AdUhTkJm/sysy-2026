#include "Expr.h"
#include "../../ir/Printer.h"
#include <algorithm>

namespace pres {

Arena Expr::ExprImpl::arena;
Expr::InternTable Expr::internTable;

void Expr::ExprImpl::dump(std::ostream &os) const {
  switch (type) {
  case Add:
  case Sub:
  case Mul:
  case Div: {
    os << "(";
    const char *op;
    switch (type) {
      case Add: op = " + "; break;
      case Sub: op = " - "; break;
      case Mul: op = " * "; break;
      case Div: op = " / "; break;
      default: assert(false && "unreachable: Expr::dump()");
    }

    assert(nops > 0);
    ops[0]->dump(os);
    for (unsigned i = 1; i < nops; i++) {
      os << op;
      ops[i]->dump(os);
    }

    os << ")";
    break;
  }

  case Parameter:
    os << ir::printer.str(v);
    break;

  case ConstInt:
    os << vi;
    break;

  case Indvar:
    os << "i";
    break;
  }
}

void Expr::dump(std::ostream &os) const {
  impl->dump(os);
}

const Expr::ExprImpl *Expr::ExprImpl::step(ir::Value *v) const {
  switch (type) {
  case Indvar:
    return ExprImpl::create(Add, ExprImpl::create(), ExprImpl::create(v));

  case Parameter:
  case ConstInt:
    return this;

  case Add:
  case Sub:
  case Mul:
  case Div: {
    ExprList vec(nops);
    for (unsigned i = 0; i < nops; i++)
      vec[i] = ops[i]->step(v);
    return intern(Key(type, nops, vec.data()));
  }
  }
}

const Expr::ExprImpl *Expr::ExprImpl::step(int v) const {
  switch (type) {
  case Indvar:
    return ExprImpl::create(Add, ExprImpl::create(), ExprImpl::create(v));

  case Parameter:
  case ConstInt:
    return this;

  case Add:
  case Sub:
  case Mul:
  case Div: {
    ExprList vec(nops);
    for (unsigned i = 0; i < nops; i++)
      vec[i] = ops[i]->step(v);
    return intern(Key(type, nops, vec.data()));
  }
  }
}

std::vector<const Expr::ExprImpl*> Expr::ExprImpl::collectAdd() const {
  if (type == ExprImpl::Add) {
    std::vector<const Expr::ExprImpl*> result;
    for (unsigned i = 0; i < nops; i++) {
      auto v = ops[i]->collectAdd();
      std::copy(v.begin(), v.end(), std::back_inserter(result));
    }
    return result;
  }

  return { this };
}

std::vector<const Expr::ExprImpl*> Expr::ExprImpl::collectMul() const {
  if (type == ExprImpl::Mul) {
    std::vector<const Expr::ExprImpl*> result;
    for (unsigned i = 0; i < nops; i++) {
      auto v = ops[i]->collectMul();
      std::copy(v.begin(), v.end(), std::back_inserter(result));
    }
    return result;
  }

  return { this };
}

const Expr::ExprImpl *Expr::ExprImpl::add(const ExprImpl *const *begin, const ExprImpl *const *end) {
  ExprList terms;
  for (auto p = begin; p != end; p++) {
    auto t = (*p)->collectAdd();
    std::copy(t.begin(), t.end(), std::back_inserter(terms));
  }

  // Do constant folding.
  int sum = 0;
  ExprList args;

  // Also identify parameters and indvars, as well as their coefficients.
  std::unordered_map<const ExprImpl *, int> coeff;

  for (auto t : terms) {
    if (t->type == ConstInt) {
      sum += t->vi;
      continue;
    }

    // Try to extract an integer coefficient from multiplicative terms of any arity.
    if (t->type == Mul) {
      int c = 1;
      ExprList nonconst;
      for (unsigned i = 0; i < t->nops; i++) {
        auto op = t->ops[i];
        if (op->type == ConstInt)
          c *= op->vi;
        else
          nonconst.push_back(op);
      }

      if (nonconst.empty()) {
        // Pure constant product.
        sum += c;
        continue;
      }

      const ExprImpl *base = nullptr;
      if (nonconst.size() == 1)
        base = nonconst[0];
      else
        // Rebuild a Mul node for the non-const factors (they're already canonicalized by flatten()/mul()).
        base = intern(Key(Mul, nonconst.size(), nonconst.data()));

      coeff[base] += c;
      continue;
    }

    // Parameter, Indvar, or any other non-Mul term: treat as base with coefficient 1.
    coeff[t] += 1;
  }

  // Synthesize `coeff`.
  for (auto [k, v] : coeff) {
    if (v == 0)
      continue;
    if (v == 1) {
      args.push_back(k);
      continue;
    }

    const ExprImpl *arr[2] = { k, create(v) };
    auto ex = intern(Key(Mul, 2, arr));
    args.push_back(ex);
  }

  if (sum != 0)
    args.push_back(create(sum));

  if (args.empty())
    return create(0);

  if (args.size() == 1)
    return args[0];

  std::sort(args.begin(), args.end());

  Key k(Add, args.size(), args.data());
  return intern(k);
}

const Expr::ExprImpl *Expr::ExprImpl::mul(const ExprImpl *const *begin, const ExprImpl *const *end) {
  ExprList terms;
  for (auto p = begin; p != end; p++) {
    auto t = (*p)->collectMul();
    std::copy(t.begin(), t.end(), std::back_inserter(terms));
  }

  // Do constant folding.
  int prod = 1;
  ExprList args;

  for (auto t : terms) {
    if (t->type == ExprImpl::ConstInt)
      prod *= t->vi;
    else
      args.push_back(t);
  }

  if (prod != 1)
    args.push_back(create(prod));

  if (args.empty())
    return create(1);

  if (args.size() == 1)
    return args[0];

  std::sort(args.begin(), args.end());

  Key k(Mul, args.size(), args.data());
  return intern(k);
}

const Expr::ExprImpl *Expr::ExprImpl::factor() const {
  if (type != Mul) {
    // Propagate the factorization.
    if (nops == 0)
      return this;

    ExprList vec(nops);
    for (unsigned i = 0; i < nops; i++)
      vec[i] = ops[i]->factor();
    return intern(Key(type, nops, vec.data()));
  }
  
  // Detect a * (b + c) and expand it.
  const ExprImpl *target = nullptr; unsigned index;
  for (unsigned i = 0; i < nops; i++) {
    auto op = ops[i];
    if (op->type == Add) {
      target = op; index = i;
      break;
    }
  }
  if (!target)
    return this;
  
  ExprList vec(target->nops);
  for (unsigned i = 0; i < target->nops; i++) {
    ExprList list(nops);
    for (unsigned j = 0; j < nops; j++) {
      if (j != index)
        list[j] = ops[j];
    }
    list[index] = target->ops[i];
    
    vec[i] = intern(Key(Mul, nops, list.data()));
  }
  return intern(Key(Add, target->nops, vec.data()));
}

const Expr::ExprImpl *Expr::ExprImpl::flatten() const {
  if (type == Add) {
    ExprList vec(nops);
    for (unsigned i = 0; i < nops; i++)
      vec[i] = ops[i]->flatten();

    return add(vec.data(), vec.data() + vec.size());
  }
  if (type == Mul) {
    ExprList vec(nops);
    for (unsigned i = 0; i < nops; i++)
      vec[i] = ops[i]->flatten();

    return mul(vec.data(), vec.data() + vec.size());
  }
  if (nops == 0)
    return this;

  ExprList vec(nops);
  for (unsigned i = 0; i < nops; i++)
    vec[i] = ops[i]->factor();
  return intern(Key(type, nops, vec.data()));
}

Expr::Key Expr::ExprImpl::asKey() const {
  switch (type) {
  case ConstInt:
    return Key(vi);
  case Parameter:
    return Key(v);
  case Indvar:
    return Key();
  default:
    return Key(type, nops, ops);
  }
}

Expr Expr::step(ir::Value *offset) const {
  if (auto vi = dyn_cast<ir::IntOp>(offset->def))
    return impl->step(vi->value);

  return impl->step(offset);
}

Expr Expr::simplify() const {
  auto x = impl, prev = x;
  do {
    prev = x;
    x = x->factor();
    x = x->flatten();
  } while (x != prev);
  return x;
}

void Expr::dropAll() {
  internTable.clear();
  ExprImpl::arena.reset();
}

Expr::Expr(ir::Value *v): impl(isa<ir::IntOp>(v->def) ? ExprImpl::create(cast<ir::IntOp>(v->def)->value) : ExprImpl::create(v)) {}

}
