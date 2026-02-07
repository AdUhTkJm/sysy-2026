#include "../Pass.h"

namespace opt {

#define make_pass_decl(Ty) make_pass(Ty);
#define low_pass_list(X) \
  X(Lower) X(RegAlloc) X(LowerPostSchedule) X(LateLegalize) X(Print) \
  X(InstCombine)

low_pass_list(make_pass_decl);
#undef make_pass_decl

}
