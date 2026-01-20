#ifndef OPS_H
#define OPS_H

#include "OpBase.h"

namespace ir {

class ModuleOp : public OpImpl<ModuleOp> {

};

#define BINARY(Ty) \
class Ty : public OpImpl<Ty> { \
public: \
  void verify(); \
  Value *lhs() const { return operands[0]; } \
  Value *rhs() const { return operands[1]; }\
};

BINARY(AddIOp);
BINARY(SubIOp);
BINARY(MulIOp);
BINARY(DivIOp);
BINARY(ModIOp);

class ReturnOp : public OpImpl<ReturnOp> {
public:
  Value *retval() const { return operands[0]; }
  void verify();
};

class PhiOp : public OpImpl<PhiOp> {
public:
  std::vector<Block*> targets;

  void verify();
};

}

#endif
