#ifndef BUILDER_H
#define BUILDER_H

#include "Ops.h"
#include "../utils/DataStructure.h"

namespace ir {

class IntOp;
class ModuleOp;
class Builder {
  Block *bb = nullptr;
  OpList::iterator at;

  void insert(Op *op);

  template <class T, class... Types, std::size_t... Is>
  void emplace_results_impl([[gnu::unused]] T* t, std::index_sequence<Is...>, Types&&... types) {
    (t->results.push_back(new Value(std::forward<Types>(types), t, Is)), ...);
  }
public:
  class Guard {
    Builder &builder;
    Block *bb;
    OpList::iterator at;
  public:
    Guard(Builder &b): builder(b), bb(b.bb), at(b.at) {}
    ~Guard() { builder.bb = bb; builder.at = at; }
  };

  void setBefore(Op *op);
  void setAfter(Op *op);
  void setToStart(Block *block);
  void setToEnd(Block *block);

  template<class T, class ...Types> __requires(
    (std::derived_from<T, OpImpl<T>> &&
    (std::derived_from<std::remove_pointer_t<Types>, Type> && ...))
  )
  T *create(Types ...types) {
    T *t = new T(bb, at);
    emplace_results_impl(t, std::index_sequence_for<Types...>{}, std::forward<Types>(types)...);
    insert(t);
    return t;
  }
  template<class T> __requires((std::derived_from<T, OpImpl<T>>))
  T *create(const std::vector<Type*> &types) {
    T *t = new T(bb, at);
    for (auto [i, ty] : data::enumerate(types))
      t->results.push_back(new Value(ty, t, i));
    insert(t);
    return t;
  }

  IntOp *createInt(int i);
  FloatOp *createFloat(float f);
  ModuleOp *createModule();
};

}
#endif
