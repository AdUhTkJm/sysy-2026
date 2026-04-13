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

  if (isa<ArrayLoadOp>(op) || isa<LoadOp>(op))
    return baseOf(op->val());

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

bool nonHoistable(Op *op);

std::set<Value*> Pass::getVariantsIn(DoWhileOp *loop) const {
  std::set<Value*> variants(loop->getResults().begin(), loop->getResults().end());
  std::set<Op*> storedGlobals;

  // To do this, we must ensure GVN is performed and PropagateArray is done, to
  // distinguish different arrays.
  bool called = false;
  bool unbased = false;
  Pass::walk(loop, [&](Op *op) {
    if (isa<ArrayStoreOp>(op) || isa<StoreOp>(op)) {
      auto base = baseOf(op->val());
      if (base)
        storedGlobals.insert(base);
      else
        unbased = true;
    }
    if (isa<CallOp>(op))
      called = true;
  });

  Pass::walk(loop, [&](Op *op) {
    bool variant = false;
    for (auto x : op->getOperands()) {
      if (variants.count(x)) {
        variant = true;
        break;
      }
    }

    if (isa<ArrayLoadOp>(op) || isa<LoadOp>(op)) {
      auto base = baseOf(op->val());
      if (unbased || !base || storedGlobals.count(base) || (isa<GlobalOp>(base) && called))
        variant = true;
    }
    if (nonHoistable(op))
      variant = true;

    if (variant) {
      for (auto r : op->getResults())
        variants.insert(r);
    }
  });

  return variants;
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

Value *Pass::increment(Value *indvar) const {
  auto loop = indvar->def;
  if (!isa<DoWhileOp>(loop))
    return nullptr;

  // Look at the condition at the end.
  auto last = condition_of(loop);
  auto index = loop->getResultIndex(indvar);
  auto next = last->val(index + 1)->def;
  if (!isa<AddIOp>(next) && !isa<AddLOp>(next))
    return nullptr;
  
  if (next->val(0) == indvar)
    return next->val(1);
  if (next->val(1) == indvar)
    return next->val(0);
  return nullptr;
}

Value *Pass::indvar(DoWhileOp *loop, ir::Value **limit) const {
  auto last = condition_of(loop);
  auto cond = last->val(0)->def;

  if (!isa<LtOp>(cond) || !isa<AddIOp>(cond->val(0)->def))
    return nullptr;

  // Generally speaking, what we're having here is `i + 'a < lim`.
  auto add = cond->val(0)->def;
  unsigned index;
  if (limit)
    *limit = cond->val(1);
  if ((index = loop->getResultIndex(add->val(0))) != loop->getNumResults())
    return add->val(0);
  else if ((index = loop->getResultIndex(add->val(1))) != loop->getNumResults())
    return add->val(1);

  return nullptr;
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