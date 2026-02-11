#ifndef OPS_H
#define OPS_H

#include "OpBase.h"

#define constructor(Ty) Ty(Block *parent, OpList::iterator place): OpImpl(parent, place) {}
#define op(Ty, ...) class Ty : public OpImpl<Ty, (int) OpKind::Ty> { \
public: \
  constructor(Ty) \
  __VA_ARGS__ \
};

#define empty_op_list(X) \
  X(ModuleOp) X(AddIOp) X(SubIOp) X(MulIOp) X(DivIOp) X(ModIOp) \
  X(AndIOp) X(OrIOp) X(XorIOp) X(ReturnOp) X(ForOp) \
  X(WhileOp) X(IfOp) X(AllocaOp) X(LoadOp) X(StoreOp) \
  X(ArrayStoreOp) X(ArrayLoadOp) X(CallOp) X(GlobalArrayOp) \
  X(GetGlobalOp) X(EqOp) X(NeOp) X(LtOp) X(LeOp) X(NotOp) \
  X(YieldOp) X(ConditionOp) X(I2FOp) X(F2IOp) X(UndefOp) \
  X(DoWhileOp) X(AddLOp) X(AddFOp) X(SubFOp) X(MulFOp) X(DivFOp) \
  /* ARM operations */ \
  X(AddWOp) X(AddXOp) \
  X(SubWOp) X(SubXOp) X(MulWOp) X(MulXOp) \
  X(DivWOp) X(DivXOp) X(CmpEqOp) X(CmpNeOp) X(CmpLtOp) X(CmpLeOp) \
  X(RetOp)

#define arm_branch_op_list(X) \
  X(BeqOp) X(BneOp) X(BltOp) X(BleOp) X(CbzOp) X(CbnzOp) X(BgeOp) X(BgtOp)

#define branch_op_list(X) \
  arm_branch_op_list(X) \
  X(BranchOp)

#define arm_targetful_op_list(X) \
  X(BOp)

#define targetful_op_list(X) \
  arm_targetful_op_list(X) \
  X(JumpOp)

#define arm_imm_op_list(X) \
  X(AddWIOp) X(AddXIOp) X(MovIOp) X(LdrOp) X(StrOp) X(LdpOp) X(StpOp) 

#define imm_op_list(X) \
  arm_imm_op_list(X) \
  X(IntOp)

#define complete_op_list(X) \
  empty_op_list(X) \
  X(BranchOp) X(JumpOp) X(PhiOp) X(IntOp) \
  X(FuncOp) X(FloatOp) X(GlobalOp) X(ExternCallOp) \
  /* ARM operations */ \
  arm_branch_op_list(X) \
  arm_imm_op_list(X) \
  X(BOp) X(BlOp) X(WriteRegOp) X(ReadRegOp) X(AdrpOp) X(AddXPOp) \

#define impure_op_list(X) \
  X(ExternCallOp) X(BranchOp) X(JumpOp) X(YieldOp) X(ConditionOp) \
  X(StoreOp) X(CallOp) X(GlobalArrayOp) X(GlobalOp) X(ReturnOp) \
  /* ARM operations */ \
  arm_branch_op_list(X) \
  X(StrOp) X(StpOp) X(BOp) X(BlOp) X(RetOp)

#define targetful_op(Ty, ...) \
  op(Ty, \
    Block *target; \
    Block *&getTarget() { return target; } \
    Block *getTarget() const { return target; } \
    __VA_ARGS__ \
  );

#define branch_op(Ty, ...) \
  targetful_op(Ty, \
    Block *other; \
    Block *&getOther() { return target; } \
    Block *getOther() const { return target; } \
    __VA_ARGS__ \
  );

#define imm_op(Ty, ...) \
  op(Ty, \
    int value; \
    int &getValue() { return value; } \
    int getValue() const { return value; } \
    __VA_ARGS__ \
  );

namespace ir {

bool hasSideEffect(Op *);

#define opkind(Ty) Ty,
enum class OpKind {
  complete_op_list(opkind)
};

empty_op_list(op)
targetful_op_list(targetful_op)
branch_op_list(branch_op)
imm_op_list(imm_op)

op(FloatOp,
  float value;
);
op(ExternCallOp,
  std::string name;
);
// The first return value is its handle,
// and the rest are function arguments.
op(FuncOp,
  std::string name;

  Value *getHandle() const;
  std::vector<Value*> getArgs() const;
  Value *getArg(unsigned i) const;
);
op(GlobalOp,
  std::string name;
);

op(PhiOp,
  std::vector<Block*> targets;

  void addIncoming(Value *v, Block *bb);
  void removeIncoming(Block *bb);
  void replaceIncoming(Block *bb, Block *after);
  Value *incomingFrom(const Block *bb) const;

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
);

/* ARM operations */
op(BlOp,
  std::string name;
);
op(WriteRegOp,
  int reg;
);
op(ReadRegOp,
  int reg;
);
op(AdrpOp,
  std::string name;
);
op(AddXPOp,
  std::string name;
);

// We need to ensure that the alignment of any subclass is no more than alignment of Op.
// This is because the custom operator new only knows the base class's alignment.
#define alignment_check(Ty) static_assert(alignof(Ty) <= alignof(Op));

complete_op_list(alignment_check)

template<class T>
struct OpKindOf { };

#define opkind_map(Ty) \
  template<> \
  struct OpKindOf<Ty> { \
    static constexpr OpKind value = OpKind::Ty; \
  };

complete_op_list(opkind_map);

extern const std::map<std::string, OpKind> opkindNames;
const char *kindname(OpKind kind);

}

#undef op
#undef opkind
#undef opkind_map
#undef branch_op
#undef imm_op
#undef targetful_op
#undef constructor
#undef alignment_check
#endif
