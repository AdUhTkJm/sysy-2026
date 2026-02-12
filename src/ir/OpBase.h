#ifndef OPBASE_H
#define OPBASE_H

#include <set>
#include <vector>
#include <map>
#include <string>
#include <list>
#include <unordered_map>

#include "../utils/DynamicCast.h"
#include "../utils/Meta.h"
#include "../utils/Alloc.h"
#include "Regs.h"

namespace ir {

class Op;
class Block;
class Attr;
class Region;

using OpList = std::list<Op*>;
using BlockList = std::list<Block*>;
using AttrMap = std::map<std::string, const Attr*>;

class Type {
public:
  enum Kind {
    i32, i64, f32, vi4, vf4, ptr, fn, unit
  } kind;
  std::vector<const Type*> subtypes;

  Type() {}
  Type(Kind kind): kind(kind) { assert(kind != ptr && kind != fn); }
  Type(Kind kind, const std::vector<const Type*> &subtypes): kind(kind), subtypes(subtypes) {}
  const Type *pointee() const { assert(subtypes.size() >= 1); return subtypes.front(); }
  const Type *retType() const { assert(subtypes.size() >= 1); return subtypes.front(); }
  auto argTypes() const { assert(subtypes.size() >= 1); return std::vector(subtypes.begin() + 1, subtypes.end()); }

  static Arena arena;
  static void* operator new(size_t) = delete;
  static void operator delete(void*) = delete;
  static void *operator new[](size_t) = delete;
  static void operator delete[](void*) noexcept = delete;
private:
  struct TypeKey {
    Kind kind;
    std::vector<const Type*> subs;

    bool operator==(const TypeKey &o) const {
      return kind == o.kind && subs == o.subs;
    }
  };
  struct TypeKeyHash {
    template <class T>
    static void hash_combine(size_t &seed, const T& v) {
      std::hash<T> hasher;
      seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
    }

    size_t operator()(const TypeKey &k) const {
      size_t h = std::hash<int>()(k.kind);
      for (auto *t : k.subs)
        hash_combine(h, (size_t) t);
      return h;
    }
  };

  using TypeCache = std::unordered_map<TypeKey, const Type*, TypeKeyHash>;
  static TypeCache cache;
public:
  static const Type *get(Kind k, const std::vector<const Type*> &subtypes) {
    TypeKey key{k, std::vector(subtypes.begin(), subtypes.end())};

    auto it = cache.find(key);
    if (it != cache.end())
      return it->second;

    void *mem = arena.allocate(sizeof(Type), alignof(Type));
    auto *t = ::new (mem) Type(k, key.subs);
    cache.emplace(std::move(key), t);
    return t;
  }
  static const Type *pointer(const Type *pointee) { return get(ptr, { pointee }); }
  static const Type *function(std::vector<const Type*> subtypes) { return get(fn, subtypes); }
  static const Type *function(const Type *ret, std::vector<const Type *> subtypes) {
    subtypes.insert(subtypes.begin(), ret);
    return get(fn, subtypes);
  }
};

extern const Type *i32, *i64, *f32, *vi4, *vf4, *unit;

class Value;
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
  friend class Builder;
public:
  static Arena arena;
  static void* operator new(size_t size) { return arena.allocate(size, alignof(Op)); }
  static void operator delete(void*) noexcept {}
  static void* operator new[](size_t) = delete;
  static void operator delete[](void*) = delete;

  const int id;
  Op(Block *parent, OpList::iterator place, int id): parent(parent), place(place), id(id) {}
  virtual ~Op();

  Op *nextOp() const;
  Op *prevOp() const;
  Op *getParentOp() const;
  Block *getParentBlock() const { return parent; }

  Value *getResult(int i = 0) const { return results[i]; }
  const auto &getResults() const { return results; }
  std::vector<const Type*> getResultTypes() const;
  size_t getNumResults() const { return results.size(); }
  size_t getNumOperands() const { return operands.size(); }

  // Implicitly append a block to the region.
  Region *appendRegion();
  void removeRegion(Region *region);
  Region *getRegion(int i = 0) const { return regions[i]; }
  size_t getNumRegions() const { return regions.size(); }

  Block *createFirstBlock();

  void pushOperand(Value *v);
  void setOperand(int i, Value *v);
  void removeOperand(int i);
  void removeOperand(Value *v);
  int  replaceOperand(Value *before, Value *after);
  void clearOperands();

  Value *pushResult(const Type *t);
  void removeResult(int i);
  void clearResults();

  bool inside(Op *op) const;
  bool inside(Block *block) const;

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

  Value *val(unsigned i = 0) const { assert(i < operands.size()); return operands[i]; }
  Value *ret(unsigned i = 0) const { assert(i < results.size()); return results[i]; }

  template<class T> __requires((std::derived_from<T, Attr>))
  const T *get() const {
    std::string name(T::getMnemonics());
    auto it = attrs.find(name);
    return it == attrs.end() ? nullptr : cast<T>(it->second);
  }

  template<class T> __requires((std::derived_from<T, Attr>))
  bool has() const {
    std::string name(T::getMnemonics());
    return attrs.count(name);
  }

  template<class T> __requires((std::derived_from<T, Attr>))
  void set(const Attr *attr) {
    std::string name(T::getMnemonics());
    attrs[name] = attr;
  }

  template<class T, class ...Args> __requires(
    (std::derived_from<T, Attr>) &&
    (std::is_constructible_v<T, Args...>)
  )
  void set(Args ...args) {
    set<T>(new T(args...));
  }

  template<class T> __requires((std::derived_from<T, Attr>))
  void remove() {
    std::string name(T::getMnemonics());
    attrs.erase(name);
  }
};


class Value {
  std::multiset<Op*> uses;
  
  friend class Op;
  template<class T, int N>
  friend class OpImpl;
  friend class Block;
public:
  const Type *const type;
  union {
    Op *const def;
    Block *const bb;
  };
  const int index;
  const bool opResult;
  const auto &getUses() const { return uses; }

  Value(const Type *type, Op *def, int index): type(type), def(def), index(index), opResult(true) {}
  Value(const Type *type, Block *bb, int index): type(type), bb(bb), index(index), opResult(false) {}

  void replaceAllUsesWith(Value *other);
  template<class F> __requires((requires(const F &f) {
    { f(std::declval<Op*>()) } -> std::same_as<bool>;
  }))
  void replaceAllUsesThat(Value *other, const F &pred) {
    for (auto it = uses.begin(); it != uses.end();) {
      if (!pred(*it)) {
        it++;
        continue;
      }

      auto next = it; next++;
      auto use = *it;
      for (auto &operand : use->operands) {
        if (operand != this)
          continue;

        operand = other;
        other->uses.insert(use);
      }
      uses.erase(it);
      it = next;
    }
  }

  bool isBlockArgument() const { return !opResult; }
  bool isOpResult() const { return opResult; }

  bool operator==(Value &other) const;
  bool used() const { return uses.size() > 0; }

  static Arena arena;
  static void* operator new(size_t size) { return arena.allocate(size, alignof(Value)); }
  static void operator delete(void*) noexcept {}
};

template<class T, int N>
class OpImpl : public Op {
public:
  static constexpr auto mnemonic = meta::name<T>();
  static const char *getMnemonics() { return mnemonic.data; }
  static constexpr size_t id = N;
  static bool classof(const Op *op) { return op->id == id; }

  OpImpl(Block *parent, OpList::iterator place): Op(parent, place, id) {}
  
  template<class ...Values> __requires((std::derived_from<std::remove_pointer_t<Values>, Value> && ...))
  T *with(Values ...values) {
    ((operands.push_back(values), values->uses.insert(this)), ...);
    return (T*) this;
  }

  T *with(const std::vector<Value*> &values) {
    for (auto x : values) {
      operands.push_back(x);
      x->uses.insert(this);
    }
    return (T*) this;
  }

  T *with(const AttrMap &map) {
    attrs = map;
    return (T*) this;
  }

  template<class Attr, class ...Args> __requires((requires(Args ...args) { Attr(args...); }))
  T *with(Args ...args) {
    attrs[T::getMnemonics()] = new Attr(args...);
    return (T*) this;
  }
};

class Block {
  OpList ops;
  Region *parent;
  BlockList::iterator place;
  AttrMap attrs;
  std::vector<Value*> args;

  std::set<Block*> doms, domFront, pdoms;
  Block *idom, *ipdom;

  friend class Op;
  friend class Region;
public:
  std::set<Block*> preds, succs;
  std::set<Value*> liveIn, liveOut;
  
  using iterator = OpList::iterator;

  Block(Region *parent, BlockList::iterator place): parent(parent), place(place) {}
  ~Block();

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
  Region *getParentRegion() const { return parent; }

  Value *pushArgument(const Type *type);
  Value *arg(unsigned i = 0) const { return args[i]; }
  size_t getNumArgs() const { return args.size(); }
  const auto &getArgs() const { return args; }
  auto &getArgs() { return args; }
  void clearArgs() { args.clear(); }

  bool dominatedBy(const Block *bb) const;
  bool dominates(const Block *bb) const { return bb->dominatedBy(this); }
  const auto &getOps() const { return ops; }
  auto &getOps() { return ops; }
  auto getNumOps() const { return ops.size(); }
  Op *getFirstOp() const { assert(ops.size() > 0); return ops.front(); }
  Op *getLastOp() const { assert(ops.size() > 0); return ops.back(); }

  const auto &getDominanceFrontier() const { return domFront; }

  void prepareErase();
  void erase();
  void rewire(Block *before, Block *after);
  
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

  Op *getLastOp() const { return getLastBlock()->getLastOp(); }

  struct MoveResult {
    Block *first, *last;
  };
  MoveResult moveTo(Block *block);

  Op *getParentOp() const { return parent; }
  auto &getBlocks() { return bbs; }
  const auto &getBlocks() const { return bbs; }
  size_t getNumBlocks() const { return bbs.size(); }
  Block *appendBlock();
  void erase();

  void updatePreds() const;
  void updateDoms() const;
  void updateDomFront() const;
  void updatePDoms() const;
  void updateLiveness() const;

  void convertToPhi();
  void convertToBlockArguments();

  iterator begin() { return bbs.begin(); }
  iterator end() { return bbs.end(); }
  Block *front() const { assert(bbs.size() >= 1 && "region: front"); return bbs.front(); }
  Block *back() const { assert(bbs.size() >= 1 && "region: back"); return bbs.back(); }
};

// Helpers.
Block *targetOf(Op *op);
Block *elseOf(Op *op);
void setTarget(Op *op, Block *bb);
void setElse(Op *op, Block *bb);

using Types = std::vector<const Type*>;
using Values = std::vector<Value*>;

extern std::unordered_map<Value*, Reg> assignment;

}

#endif
