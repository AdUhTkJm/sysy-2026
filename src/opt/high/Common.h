#include "../Pass.h"           // IWYU pragma: keep
#include "../../ir/Builder.h"  // IWYU pragma: keep
#include "../../ir/Attrs.h"    // IWYU pragma: keep
#include "../../ir/Printer.h"  // IWYU pragma: keep
#include "../../utils/Interval.h"

using namespace ir;

namespace opt {

using RangeResult = std::unordered_map<const Op*, data::ConstEnv>;
extern RangeResult rangeResult;

}
