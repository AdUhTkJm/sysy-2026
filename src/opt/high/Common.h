#include "../Pass.h"           // IWYU pragma: keep
#include "../../ir/Builder.h"  // IWYU pragma: keep
#include "../../ir/Attrs.h"    // IWYU pragma: keep
#include "../../ir/Printer.h"  // IWYU pragma: keep

using namespace ir;

namespace opt {

std::set<Value*> getVariantsIn(DoWhileOp *loop);

}
