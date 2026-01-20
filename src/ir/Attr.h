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

class TargetAttr : public AttrImpl<TargetAttr> {
public:
  Block *bb;
};

class ElseAttr : public AttrImpl<ElseAttr> {
public:
  Block *bb;
};

}

#endif