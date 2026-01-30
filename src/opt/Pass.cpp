#include "Pass.h"

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

void PassManager::run() {
  for (auto pass : passes) {
    if (options.printBefore == pass->name())
      std::cout << module;
    pass->run();
    if (options.printAfter == pass->name())
      std::cout << module;
  }
}

void PassManager::addPass(Pass *pass) {
  passes.push_back(pass);
}

}