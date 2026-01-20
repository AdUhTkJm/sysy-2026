#ifndef OPBASE_H
#define OPBASE_H

#include <set>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <list>
#include <iostream>

#include "../utils/DynamicCast.h"
#include "../utils/Meta.h"

namespace ir {

class Op;
class Block;
class Attr;
class Region;

using OpList = std::list<Op*>;
using BlockList = std::list<Block*>;
using AttrMap = std::map<std::string, Attr*>;

struct Type {
  enum Kind {
    i32, i64, f32, vi4, vf4, ptr, fn
  } kind;
  std::vector<Type*> subtypes;

  Type *pointee() const;
  Type *retType() const;
  std::vector<Type*> argTypes() const;
};

class Value {
  std::multiset<Op*> uses;
  
  friend class Op;
public:
  Type *type;
  Op *def;
  int index;
  void replaceAllUsesWith(Value *other);

  bool operator==(Value &other) const;
};

class Op {
protected:
  std::vector<Value*> results;
  std::vector<Value*> operands;
  std::vector<Region*> regions;
  AttrMap attrs;
  Block *parent;
  OpList::iterator place;

  friend class Value;
  friend class Block;
  friend class Region;
public:
  const unsigned long id;
  Op(Block *parent, OpList::iterator place, unsigned long id): parent(parent), place(place), id(id) {}

  Op *nextOp() const;
  Op *prevOp() const;
  Op *getParentOp() const;

  Value *getResult(int i = 0) { return results[i]; }
  const auto &getResults() { return results; }

  Region *appendRegion();
  void removeRegion(Region *region);

  Block *createFirstBlock();

  void pushOperand(Value *v);
  void setOperand(int i, Value *v);
  void removeOperand(int i);
  void removeOperand(Value *v);
  int  replaceOperand(Value *before, Value *after);
  void clearOperands();

  bool inside(Op *op) const;

  void moveBefore(Op *op);
  void moveAfter(Op *op);
  void moveToStart(Block *block);
  void moveToEnd(Block *block);

  void clearAttributes();
  void removeAttribute(const std::string &name);
  void setAttribute(const std::string &name, Attr *attr);

  void erase();

  auto &getRegions() { return regions; }
  auto &getAttrs() { return attrs; }
  const auto &getRegions() const { return regions; }
  const auto &getOperands() const { return operands; }
  const auto &getAttrs() const { return attrs; }

  template<class T>
  T *get() {
    std::string name(T::getMnemonics());
    auto it = attrs.find(name);
    return it == attrs.end() ? nullptr : cast<T>(it->second);
  }

  template<class T>
  void set(Attr *attr) {
    std::string name(T::getMnemonics());
    attrs[name] = attr;
  }

  // Panicks if verification fails.
  // No need to call verify() on sub-operations inside this function;
  // That is handled separately.
  virtual void verify() {}
};

template<class T>
class OpImpl : public Op {
  static char *unique() {
    static char p;
    return &p;
  }
public:
  static constexpr auto mnemonic = meta::name<T>();
  static const char *getMnemonics() { return mnemonic.data(); }
  static bool classof(const Op *op) { return op->id == (unsigned long) unique(); }

  OpImpl(Block *parent, OpList::iterator place): Op(parent, place, (unsigned long) unique()) {}
};

class Block {
  OpList ops;
  Region *parent;
  BlockList::iterator place;

  std::set<Block*> doms, domFront, pdoms;
  std::set<Value*> liveIn, liveOut;
  Block *idom, *ipdom;

  friend class Op;
  friend class Region;
public:
  std::set<Block*> preds, succs;
  using iterator = OpList::iterator;

  Block(Region *parent, BlockList::iterator place): parent(parent), place(place) {}

  void insert(iterator at, Op *op);
  void insertAfter(iterator at, Op *op);

  void inlineToEnd(Block *block);
  void inlineBefore(Op *op);

  void splitOpsAfter(Block *dest, Op *op);
  void splitOpsBefore(Block *dest, Op *op);

  void moveBefore(Block *block);
  void moveAfter(Block *block);
  void moveToEnd(Region *region);

  void remove(iterator at);
  Block *nextBlock() const;

  bool dominatedBy(const Block *bb) const;
  bool dominates(const Block *bb) const { return bb->dominatedBy(this); }
  auto getOps() { return ops; }
  auto getOpCount() { return ops.size(); }
  Op *getFirstOp() { return ops.front(); }
  Op *getLastOp() { return ops.back(); }

  const auto &getDominanceFrontier() { return domFront; }

  void erase();
  
  iterator begin() { return ops.begin(); }
  iterator end() { return ops.end(); }
};

class Region {
  BlockList bbs;
  Op *parent;

  friend class Op;
  friend class Block;
public:
  using iterator = BlockList::iterator;

  Region(Op *parent): parent(parent) {}

  void remove(Block *block);
  void remove(iterator at);

  Block *insert(Block *block);
  Block *insertAfter(Block *block);
  void insert(iterator at, Block *block);
  void insertAfter(iterator at, Block *block);

  Block *getFirstBlock() const { return bbs.front(); }
  Block *getLastBlock() const { return bbs.back(); }

  struct MoveResult {
    Block *first, *last;
  };
  MoveResult moveTo(Block *block);

  Op *getParentOp() const { return parent; }
  auto &getBlocks() { return bbs; }
  const auto &getBlocks() const { return bbs; }
  Block *appendBlock();
  void erase();

  void updatePreds() const;
  void updateDoms() const;
  void updateDomFront() const;
  void updatePDoms() const;
  void updateLiveness() const;

  iterator begin() { return bbs.begin(); }
  iterator end() { return bbs.end(); }
};

class Printer {
  std::unordered_map<Block *, int> blockid;
  std::unordered_map<Value *, int> valueid;
  std::ostream &os;
  int depth = 0, bid = 0, vid = 0;
public:
  Printer(std::ostream &os): os(os) {}
  int getBlockID(Block *block);
  int getValueID(Value *value);

  void print(Region *region);
  void print(Block *block);
  void print(Op *op);
};
  
}

#endif
