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

  template <class T, class... Types>
  void emplaceResultsImpl([[gnu::unused]] T* t, Types&&... types) {
    (t->results.push_back(new Value(std::forward<Types>(types), t)), ...);
  }

  void replaceImpl(Op *t, Op *other) {
    assert(t->getNumResults() == other->getNumResults());
    for (auto [i, result] : data::enumerate(other->getResults()))
      result->replaceAllUsesWith(t->ret(i));
    other->erase();
  }

  Op *cloneImpl(Op *);
public:
  class Guard {
    Builder &builder;
    Block *bb;
    OpList::iterator at;
  public:
    Guard(Builder &b): builder(b), bb(b.bb), at(b.at) {}
    ~Guard() { builder.bb = bb; builder.at = at; }
  };

  Builder() {}
  Builder(Op *op) { setBefore(op); }

  // Maps from the original value to the copied value.
  using Map = std::map<Value *, Value *>;
  using OpMap = std::map<Op *, Op *>;

  void setBefore(Op *op);
  void setAfter(Op *op);
  void setToStart(Block *block);
  void setToEnd(Block *block);
  void setToStart(Region *region) { setToStart(region->getFirstBlock()); }
  void setToEnd(Region *region) { setToEnd(region->getLastBlock()); }

  template<class T, class ...Types> __requires(
    (std::derived_from<T, Op> &&
    (std::derived_from<std::remove_pointer_t<Types>, Type> && ...))
  )
  T *create(Types ...types) {
    T *t = new T(bb, at);
    emplaceResultsImpl(t, std::forward<Types>(types)...);
    insert(t);
    return t;
  }
  template<class T> __requires((std::derived_from<T, Op>))
  T *create(const std::vector<const Type*> &types) {
    T *t = new T(bb, at);
    for (auto [i, ty] : data::enumerate(types))
      t->results.push_back(new Value(ty, t));
    insert(t);
    return t;
  }

  template<class T, class ...Types> __requires(
    (std::derived_from<T, Op> &&
    (std::derived_from<std::remove_pointer_t<Types>, Type> && ...))
  )
  T *replace(Op *other, Types ...types) {
    setBefore(other);
    T *t = create<T>(std::forward<Types>(types)...);
    replaceImpl(t, other);
    return t;
  }
  template<class T> __requires((std::derived_from<T, Op>))
  T *replace(Op *other, const std::vector<const Type*> &types) {
    setBefore(other);
    T *t = create<T>(types);
    replaceImpl(t, other);
    return t;
  }

  template<class T> __requires((std::derived_from<T, Op>))
  T *rename(Op *other) {
    setBefore(other);
    T *t = create<T>(other->getResultTypes())->with(other->operands);
    for (auto region : other->getRegions())
      t->regions.push_back(region);
    replaceImpl(t, other);
    return t;
  }

  void copy(Block *bb, Map &map, OpMap &opmap);
  Op *clone(Op *op);
  Op *clone(Op *op, Map &map);
  Op *clone(Op *op, OpMap &opmap);
  Op *clone(Op *op, Map &map, OpMap &opmap);

  IntOp *createInt(int i);
  FloatOp *createFloat(float f);
  ModuleOp *createModule();
};

}
#endif
