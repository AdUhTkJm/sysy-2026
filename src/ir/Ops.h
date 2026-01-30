#ifndef OPS_H
#define OPS_H

#include "OpBase.h"

#define constructor(Ty) Ty(Block *parent, OpList::iterator place): OpImpl(parent, place) {}
#define op(Ty) class Ty : public OpImpl<Ty> { \
public: \
  constructor(Ty) \
};

#define empty_op_list(X) \
  X(ModuleOp) X(AddIOp) X(SubIOp) X(MulIOp) X(DivIOp) X(ModIOp) \
  X(AndIOp) X(OrIOp) X(XorIOp) X(ReturnOp) X(ForOp) \
  X(WhileOp) X(IfOp) X(AllocaOp) X(LoadOp) X(StoreOp) \
  X(ArrayStoreOp) X(ArrayLoadOp) X(CallOp) X(GlobalArrayOp) \
  X(GetGlobalOp) X(EqOp) X(NeOp) X(LtOp) X(LeOp) X(NotOp) \
  X(YieldOp) X(ConditionOp) X(I2FOp) X(F2IOp) X(UndefOp) \
  X(DoWhileOp) \

#define complete_op_list(X) \
  empty_op_list(X) X(BranchOp) X(JumpOp) X(PhiOp) X(IntOp) \
  X(FuncOp) X(FloatOp) X(GlobalOp) X(ExternCallOp) \

namespace ir {

empty_op_list(op)

class IntOp : public OpImpl<IntOp> {
public:
  constructor(IntOp);
  int value;
};

class FloatOp : public OpImpl<FloatOp> {
public:
  constructor(FloatOp);
  float value;
};

class BranchOp : public OpImpl<BranchOp> {
public:
  constructor(BranchOp);
  Block *target, *other;
};

class JumpOp : public OpImpl<JumpOp> {
public:
  constructor(JumpOp);
  Block *target;
};

class ExternCallOp : public OpImpl<ExternCallOp> {
public:
  constructor(ExternCallOp);
  std::string name;
};

// The first return value is its handle,
// and the rest are function arguments.
class FuncOp : public OpImpl<FuncOp> {
public:
  constructor(FuncOp);
  std::string name;

  Value *getHandle() const;
  std::vector<Value*> getArgs() const;
  Value *getArg(unsigned i) const;
};

class GlobalOp : public OpImpl<GlobalOp> {
public:
  constructor(GlobalOp);
  std::string name;
};

class PhiOp : public OpImpl<PhiOp> {
public:
  constructor(PhiOp);
  std::vector<Block*> targets;

  class iterator {
    PhiOp *phi;
    size_t i;

    iterator(PhiOp *op, size_t i): phi(op), i(i) {}
    friend class PhiOp;
  public:
    iterator &operator++() {
      ++i;
      return *this;
    }

    iterator operator++(int) {
      auto p = *this;
      i++;
      return p;
    }

    iterator operator--() {
      --i;
      return *this;
    }

    iterator operator--(int) {
      auto p = *this;
      i--;
      return p;
    }

    bool operator==(const iterator &other) const {
      return phi == other.phi && i == other.i;
    }
    
    bool operator!=(const iterator &other) const {
      return !(*this == other);
    }

    std::pair<Value *, Block *> operator*() {
      return std::make_pair(phi->operands[i], phi->targets[i]);
    }
  };

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, targets.size()); }
};

// We need to ensure that the alignment of any subclass is no more than alignment of Op.
// This is because the custom operator new only knows the base class's alignment.
#define alignment_check(Ty) static_assert(alignof(Ty) <= alignof(Op));

complete_op_list(alignment_check)

}

#undef op
#undef constructor
#undef alignment_check
#endif
