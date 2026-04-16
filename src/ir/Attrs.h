#ifndef ATTR_H
#define ATTR_H

#include "../utils/Meta.h"
#include "../utils/DataStructure.h"
#include "../utils/Alloc.h"
#include "../utils/presburger/AffineFunction.h"
#include <numeric>
#include <map>

namespace ir {

#define attr_list(X) \
  X(IntAttr) X(SizeAttr) X(DimAttr) X(ArgDimAttr) \
  X(ConstIArrAttr) X(ConstFArrAttr) X(UnerasableAttr) X(RecursiveAttr) \
  X(IncomingStackArgAttr) X(NonIdempotentAttr) X(UnreachableAttr) X(StackOffsetAttr) \
  X(SubscriptAttr)

class Block;
class Value;
  
class Attr {
public:
  static Arena arena;
  static void* operator new(size_t size) { return arena.allocate(size, alignof(Attr)); }
  static void operator delete(void*) noexcept {}
  static void* operator new[](size_t) = delete;
  static void operator delete[](void*) = delete;

  const unsigned long id;
  Attr(unsigned long id): id(id) {}
};

inline Arena Attr::arena;

template<class T>
class AttrImpl : public Attr {
  static char *unique() {
    static char p;
    return &p;
  }
public:
  static unsigned long identifier() { return (unsigned long) unique(); }
  static constexpr auto mnemonic = meta::name<T>();
  static const char *getMnemonics() { return mnemonic.data; }
  static bool classof(const Attr *attr) { return attr->id == identifier(); }

  AttrImpl(): Attr(identifier()) {}
};

class IntAttr : public AttrImpl<IntAttr> {
public:
  int i;
  IntAttr(int i): i(i) {}
};

class SizeAttr : public AttrImpl<SizeAttr> {
public:
  size_t size;
  SizeAttr(size_t size): size(size) {}
};

// This is for a particular alloca.
class DimAttr : public AttrImpl<DimAttr> {
public:
  std::vector<int> dims;
  DimAttr(const std::vector<int> &dims): dims(dims) {}

  int size() const {
    return std::accumulate(dims.begin(), dims.end(), 1, [](int v, int x) { return v * x; });
  }
};

// This is for functions, and annotates its arguments.
class ArgDimAttr : public AttrImpl<ArgDimAttr> {
public:
  std::map<Value*, std::vector<int>> dims;
  ArgDimAttr(const std::map<Value*, std::vector<int>> &dims): dims(dims) {}
};

class ConstIArrAttr : public AttrImpl<ConstIArrAttr> {
public:
  std::vector<int> value;
  size_t zeroSuffix;

  ConstIArrAttr(const std::vector<int> &value): value(value), zeroSuffix(0) {
    for (auto x : data::reverse(value)) {
      if (x != 0)
        break;
      zeroSuffix++;
    }
  }

  bool allZeroes() const { return zeroSuffix == value.size(); }
};

class ConstFArrAttr : public AttrImpl<ConstFArrAttr> {
public:
  std::vector<float> value;
  size_t zeroSuffix;

  ConstFArrAttr(const std::vector<float> &value): value(value), zeroSuffix(0) {
    for (auto x : data::reverse(value)) {
      if (x != 0)
        break;
      zeroSuffix++;
    }
  }

  bool allZeroes() const { return zeroSuffix == value.size(); }
};

// Empty.
class UnerasableAttr : public AttrImpl<UnerasableAttr> {};
class NonIdempotentAttr : public AttrImpl<NonIdempotentAttr> {};
class RecursiveAttr : public AttrImpl<RecursiveAttr> {};
// Marks `LdrOp` of a function parameter passed on the stack (offset patched in LateLegalize).
class IncomingStackArgAttr : public AttrImpl<IncomingStackArgAttr> {};

// Marks the additional stack offset introduced by stack arguments for calls in the function.
class StackOffsetAttr : public AttrImpl<StackOffsetAttr> {
public:
  int size;
  StackOffsetAttr(int i): size(i) {}
};

// The i'th region of the targeted operation is unreachable.
class UnreachableAttr : public AttrImpl<UnreachableAttr> {
public:
  int region;
  UnreachableAttr(int i): region(i) {}
};

class SubscriptAttr : public AttrImpl<SubscriptAttr> {
public:
  std::vector<pres::AffineFunction> subscripts;
  SubscriptAttr(const std::vector<pres::AffineFunction> &subscripts):
    subscripts(subscripts) {}
};

using Attributes = std::vector<const Attr*>;

}

#endif