#ifndef EXPR_H
#define EXPR_H

#include "../Alloc.h"
#include "../../opt/Pass.h"
#include <iostream>
#include <unordered_map>
#include <vector>

namespace ir {

class Value;

}

namespace pres {

using opt::Preorder;
using opt::Postorder;

// A thin wrapper around ExprImpl. This is because `const ExprImpl*` cannot support overloading.
class Expr {
public:
  struct ExprImpl {
    enum Type {
      Add, Sub, Mul, Div, Parameter, Indvar, ConstInt
    } type;
    unsigned nops;
    union {
      const ExprImpl *ops[1];
      ir::Value *v;
      int vi;
    };

    static Arena arena;
    static void* operator new(size_t size, unsigned nops) {
      return arena.allocate(size + (nops > 0 ? (nops - 1) * sizeof(const ExprImpl*) : 0), alignof(ExprImpl));
    }
    static void operator delete(void*) noexcept {}
    static void* operator new[](size_t) = delete;
    static void operator delete[](void*) = delete;

    template<typename... Args>
    static const ExprImpl* create(Type t, Args... args) {
      constexpr size_t N = sizeof...(Args);
      const ExprImpl* arr[] = { args... };
      return intern(Key(t, N, arr));
    }

    static const ExprImpl *create() { return intern(Key()); }
    static const ExprImpl *create(ir::Value *v) { return intern(Key(v)); }
    static const ExprImpl *create(int v) { return intern(Key(v)); }
    static ExprImpl *createSkeleton(Type t, size_t n) {
      auto ex = new(n) ExprImpl(t);
      ex->nops = n;
      return ex;
    }

    struct Key {
      Type type;
      unsigned long nops;
      union {
        const ExprImpl *const *ops;
        ir::Value* v;
        int vi;
      };

      Key(Type ty, unsigned nops, const ExprImpl *const *ops): type(ty), nops(nops), ops(ops) {}
      Key(ir::Value *v): type(Parameter), nops(0), v(v) {}
      Key(int v): type(ConstInt), nops(0), vi(v) {}
      Key(): type(Indvar), nops(0) {}
    };

    struct KeyEq {
      bool operator()(const Key& a, const Key& b) const {
        if (a.type != b.type)
          return false;

        if (a.type == ExprImpl::Parameter)
          return a.v == b.v;

        if (a.type == ExprImpl::ConstInt)
          return a.vi == b.vi;

        if (a.nops != b.nops)
          return false;

        for (unsigned i = 0; i < a.nops; i++)
          if (a.ops[i] != b.ops[i])
            return false;

        return true;
      }
    };

    struct KeyHash {
      static size_t hash_combine(size_t a, size_t b) {
        return a ^ (b + 0x9e3779b97f4a7c15ULL + (a<<6) + (a>>2));
      }

      size_t operator()(const Key &k) const {
        size_t h = std::hash<int>()(k.type);

        if (k.type == ExprImpl::Parameter)
          return hash_combine(h, std::hash<void*>()(k.v));

        if (k.type == ExprImpl::ConstInt)
          return hash_combine(h, std::hash<int>()(k.vi));

        if (k.type == ExprImpl::Indvar)
          return true;

        for (unsigned i = 0; i < k.nops; i++)
          h = hash_combine(h, std::hash<const void*>()(k.ops[i]));

        return h;
      }
    };

    static const ExprImpl *intern(Key k) {
      auto it = internTable.find(k);
      if (it != internTable.end())
        return it->second;

      auto *node = ExprImpl::createSkeleton(k.type, k.nops);
      if (k.type == ExprImpl::Parameter)
        node->v = k.v;
      else if (k.type == ExprImpl::ConstInt)
        node->vi = k.vi;
      else if (k.type != ExprImpl::Indvar) {
        for (unsigned i = 0; i < k.nops; i++)
          node->ops[i] = k.ops[i];
        k.ops = node->ops;
      }

      internTable.emplace(k, node);
      return node;
    }

    void dump(std::ostream &os) const;
    const ExprImpl *step(ir::Value *v) const;
    const ExprImpl *step(int v) const;

    std::vector<const ExprImpl*> collectAdd() const;
    std::vector<const ExprImpl*> collectMul() const;

    static const ExprImpl *add(const ExprImpl *l, const ExprImpl *r) {
      const ExprImpl *arr[2] = { l, r };
      return add(arr, arr + 2);
    }
    static const ExprImpl *mul(const ExprImpl *l, const ExprImpl *r) {
      const ExprImpl *arr[2] = { l, r };
      return mul(arr, arr + 2);
    }

    static const ExprImpl *add(const ExprImpl *const *begin, const ExprImpl *const *end);
    static const ExprImpl *mul(const ExprImpl *const *begin, const ExprImpl *const *end);

    const ExprImpl *factor() const;
    const ExprImpl *flatten() const;

    Key asKey() const;

    template<class T = Postorder, class F> __requires(opt::walk_order<T> && std::invocable<F, const ExprImpl *>)
    void walk(const F &f) const { walkImpl(f, T{}); }
  private:
    template<class ...Args>
    ExprImpl(Type ty, Args... args): type(ty), nops(sizeof...(Args)) {
      constexpr size_t N = sizeof...(Args);
      const ExprImpl *arr[] = { args... };
      for (unsigned i = 0; i < N; i++)
        ops[i] = arr[i];
    }

    template<class F>
    void walkImpl(const F &f, Postorder) const {
      for (unsigned i = 0; i < nops; i++)
        ops[i]->walkImpl(f, Postorder{});
      f(this);
    }

    template<class F>
    void walkImpl(const F &f, Preorder) const {
      f(this);
      for (unsigned i = 0; i < nops; i++)
        ops[i]->walkImpl(f, Preorder{});
    }
  };
private:
  using InternTable = std::unordered_map<ExprImpl::Key, const ExprImpl*, ExprImpl::KeyHash, ExprImpl::KeyEq>;
  static InternTable internTable;
public:
  const ExprImpl *impl;
  Expr(const ExprImpl *impl): impl(impl) {}

  using Key = ExprImpl::Key;
  using Type = ExprImpl::Type;
  using ExprList = std::vector<const ExprImpl*>;

  Expr(): impl(ExprImpl::create()) {}
  Expr(ir::Value *v);
  Expr(int v): impl(ExprImpl::create(v)) {}

  Expr operator+(const Expr &other) const { return ExprImpl::add(impl, other.impl); }
  Expr operator-(const Expr &other) const { return ExprImpl::add(impl, ExprImpl::mul(ExprImpl::create(-1), other.impl)); }
  Expr operator*(const Expr &other) const { return ExprImpl::mul(impl, other.impl); }
  Expr operator/(const Expr &other) const { return ExprImpl::create(ExprImpl::Div, impl, other.impl); }

  void dump(std::ostream &os) const;
  Expr step(ir::Value *offset) const;
  Expr simplify() const;

  template<class F> __requires((std::invocable<F, const ExprImpl *>))
  void walk(const F &f) const { impl->walk(f); }

  static void dropAll();
};

inline std::ostream &operator<<(std::ostream &os, const Expr &expr) {
  expr.dump(os);
  return os;
}

}

#endif
