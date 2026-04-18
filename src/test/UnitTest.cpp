#include "UnitTest.h"
#include <cassert>

namespace test {

void runIntegerRelationTests();
void runFractionTests();

void runUnitTest(const std::string &name) {
  if (name == "intrel") {
    runIntegerRelationTests();
    return;
  }

  if (name == "frac") {
    runFractionTests();
    return;
  }

  if (name == "all") {
    runIntegerRelationTests();
    runFractionTests();
    return;
  }

  assert(false && "unknown name");
}

}
