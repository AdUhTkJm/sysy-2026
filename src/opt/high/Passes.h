#include "../Pass.h"

namespace opt {

#define make_pass_decl(Ty) make_pass(Ty);
#define high_pass_list(X) \
  X(Mem2Reg) X(EnsureTerminator) X(HighDCE) X(Pure) X(PropagateArray) \
  X(RaiseArray) X(Recursive) X(TidyCodeGen) X(Fold) X(HighGVN) \
  X(InlineGlobalStore) X(LICM) X(LowerArray) X(SCEV) X(ADCE) X(Inline) \
  X(FoldConstGlobal) X(Range) X(RangedFold) X(TCO)

high_pass_list(make_pass_decl);
#undef make_pass_decl

}
