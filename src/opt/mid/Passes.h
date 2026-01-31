#include "../Pass.h"

namespace opt {

#define make_pass_decl(Ty) make_pass(Ty);
#define mid_pass_list(X) \
  X(Flatten)

mid_pass_list(make_pass_decl);
#undef make_pass_decl

}
