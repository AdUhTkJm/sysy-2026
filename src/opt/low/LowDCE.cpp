#include "Common.h"
#include <algorithm>

namespace opt {

#define removable_op_list(X) \
  X(AddXIOp) X(AddWIOp) X(AddWOp) X(AddLOp) X(AddXOp) X(AddXPOp) X(AdrpOp) \
  X(CmpEqOp) X(CmpLeOp) X(CmpLtOp) X(CmpNeOp) X(MovIOp) X(LslWOp) X(LslWIOp) \
  X(SubWOp) X(SubXOp) X(SubWIOp) X(SubXIOp) X(MulWOp) X(MulXOp) X(DivWOp) \
  X(DivXOp) X(LdrOp) X(LdrLslOp) X(MovIOp) X(EorWOp) X(EorWIOp)

#define removable_decl(Ty) isa<Ty>(op) ||

static bool removable(Op *op) {
  return removable_op_list(removable_decl) false;
}

declare_local_pass(LowDCE) {
  fixed(walk<Postorder>(func, [&](Op *op) {
    if (std::all_of(op->getResults().begin(), op->getResults().end(), [](Value *v){
      return !v->used();
    }) && removable(op)) {
      op->erase();
      mark_changed;
    }
  }););
}

}