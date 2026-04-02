#include "Pass.h"
#include "../main/Options.h"
#include "../ir/Printer.h"
#include "../ir/Interpreter.h"

using namespace ir;

namespace opt {

std::vector<FuncOp*> Pass::collectFunctions() const {
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

bool PassManager::shouldPrintBefore(const char *name) const {
  return options.printAll ||
    (!strcmp_nocase(options.printBefore.c_str(), name) &&
    (options.beforeIndex == 0 || options.beforeIndex == time.at(name)));
}

bool PassManager::shouldPrintAfter(const char *name) const {
  return options.printAll ||
    (!strcmp_nocase(options.printAfter.c_str(), name) &&
    (options.afterIndex == 0 || options.afterIndex == time.at(name)));
}

void PassManager::run() {
  for (auto pass : passes) {
    auto name = pass->name();
    time[name]++;
    if (options.printAll)
      std::cerr << "===== " << name << " =====\n";
    if (shouldPrintBefore(name))
      std::cerr << module;
    
    pass->run();

    if (shouldPrintAfter(name))
      std::cerr << module;

    if (options.interpret) {
      Interpreter interp;
      ExecResult res = interp.execute(module);
      assert(res.values.size() == 1 && res.kind == ExecResult::Return);
      std::cerr << res.values[0].vi << "\n";
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

FuncOp *Pass::findFunction(std::string_view name) const {
  for_all(FuncOp) {
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

  if (isa<AddXOp>(op) || isa<AddLOp>(op)) {
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

Pass::CallGraph Pass::calledGraph() const {
  CallGraph cg;
  for (auto fn : collectFunctions()) {
    cg[fn]; // Default-construct.
    for_all(CallOp, fn)
      cg[cast<FuncOp>(op->val()->def)].insert(fn);
  }
  return cg;
}

Pass::CallGraph Pass::callGraph() const {
  CallGraph cg;
  for (auto fn : collectFunctions()) {
    cg[fn]; // Default-construct.
    for_all(CallOp, fn)
      cg[fn].insert(cast<FuncOp>(op->val()->def));
  }
  return cg;
}

void Pass::checkAssignmentLegality(Op *parent) const {
  walk(parent, [](Op *op) {
    if (isa<GlobalOp>(op) || isa<FuncOp>(op) || isa<AllocaOp>(op))
      return;

    for (auto v : op->getResults()) {
      if (!assignment.count(v) || assignment[v] == unallocated) {
        std::cerr << "unallocated: " << op;
        assert(false);
      }
    }
  });
}

}