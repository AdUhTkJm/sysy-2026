#ifndef ATTR_H
#define ATTR_H

#include "../utils/Meta.h"
#include "../utils/DataStructure.h"
#include "../utils/Alloc.h"
#include <numeric>

namespace ir {

#define attr_list(X) \
  X(IntAttr) X(SizeAttr) X(DimAttr) X(ConstIArrAttr) X(ConstFArrAttr) X(ImpureAttr)

class Block;
  
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

class DimAttr : public AttrImpl<DimAttr> {
public:
  std::vector<int> dims;
  DimAttr(const std::vector<int> &dims): dims(dims) {}

  int size() const {
    return std::accumulate(dims.begin(), dims.end(), 1, [](int v, int x) { return v * x; });
  }
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
class ImpureAttr : public AttrImpl<ImpureAttr> {};

using Attributes = std::vector<const Attr*>;

}

#endif