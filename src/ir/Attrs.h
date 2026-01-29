#ifndef ATTR_H
#define ATTR_H

#include "../utils/Meta.h"

namespace ir {

class Block;
  
class Attr {
public:
  const unsigned long id;
  Attr(unsigned long id): id(id) {}
};

template<class T>
class AttrImpl : public Attr {
  static char *unique() {
    static char p;
    return &p;
  }
public:
  static constexpr auto mnemonic = meta::name<T>();
  static const char *getMnemonics() { return mnemonic.data; }
  static bool classof(const Attr *attr) { return attr->id == (unsigned long) unique(); }

  AttrImpl(): Attr((unsigned long) unique()) {}
};

class IntAttr : public AttrImpl<IntAttr> {
public:
  int i;
  IntAttr(int i): i(i) {}
};

}

#endif