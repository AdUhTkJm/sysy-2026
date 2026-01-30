#include "../Pass.h"

namespace opt {

#define make_pass_decl(Ty) make_pass(Ty);
#define pass_list(X) \
  X(Mem2Reg) X(EnsureTerminator)

pass_list(make_pass_decl);
#undef make_pass_decl

}
