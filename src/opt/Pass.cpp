#include "Pass.h"
#include "../main/Options.h"
#include "../ir/Printer.h"

using namespace ir;

namespace opt {

std::vector<FuncOp*> Pass::collectFunctions() {
  std::vector<FuncOp*> result;
  auto region = module->getRegion();
  assert(region->getNumBlocks() == 1);
  for (auto op : *region->getFirstBlock()) {
    if (auto fn = dyn_cast<FuncOp>(op))
      result.push_back(fn);
  }
  return result;
}

int strcmp_nocase(const char *l, const char *r) {
  auto p = l, q = r;
  while (*p && *q) {
    int diff = tolower(*p++) - tolower(*q++);
    if (diff < 0)
      return -1;
    if (diff > 0)
      return 1;
  }
  return *p ? 1 : *q ? -1 : 0;
}

void PassManager::run() {
  for (auto pass : passes) {
    if (!strcmp_nocase(options.printBefore.c_str(), pass->name()))
      std::cout << module;
    pass->run();
    if (!strcmp_nocase(options.printAfter.c_str(), pass->name()))
      std::cout << module;
  }
}

void PassManager::addPass(Pass *pass) {
  passes.push_back(pass);
}

}