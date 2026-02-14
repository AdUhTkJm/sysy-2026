#include "Pass.h"
#include "../main/Options.h"
#include "../ir/Printer.h"
#include "../ir/Interpreter.h"

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
    if (options.printAll || !strcmp_nocase(options.printBefore.c_str(), pass->name()))
      std::cout << module;
    
    pass->run();

    if (options.printAll || !strcmp_nocase(options.printAfter.c_str(), pass->name()))
      std::cout << module;

    if (options.interpret) {
      Interpreter interp;
      ExecResult res = interp.execute(module);
      assert(res.values.size() == 1 && res.kind == ExecResult::Return);
      std::cout << res.values[0].vi << "\n";
    }
  }
}

void PassManager::addPass(Pass *pass) {
  passes.push_back(pass);
}

GlobalOp *Pass::findGlobal(std::string_view name) const {
  for_all(GlobalOp) {
    if (op->name == name)
      return op;
  }

  return nullptr;
}

Op *Pass::directBaseOf(Op *op) const {
  if (isa<AllocaOp>(op))
    return op;

  if (isa<GetGlobalOp>(op))
    return op->val()->def;

  assert(false && "there is no base!");
  return nullptr;
}

Op *Pass::baseOf(Value *v) const {
  auto op = v->def;
  if (!op)
    return nullptr;

  if (isa<AllocaOp>(op) || isa<FuncOp>(op))
    return op;

  if (isa<GetGlobalOp>(op))
    return op->val()->def;

  if (isa<AddXOp>(op)) {
    if (auto k = baseOf(op->val(0)))
      return k;
    
    return baseOf(op->val(1));
  }

  if (auto addxp = dyn_cast<AddXPOp>(op))
    return findGlobal(addxp->name);

  if (isa<FuncOp>(op))
    return op;
  
  return nullptr;
}

}