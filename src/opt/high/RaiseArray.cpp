#include "Common.h"
#include "../../utils/SCC.h"

namespace opt {

declare_pass(RaiseArray,
  void raiseRecursive(FuncOp *func);
  void runImpl(FuncOp *func);

  data::Tarjan *tarjan;
) {
  auto cg = callGraph();
  data::Tarjan t(cg);
  tarjan = &t;

  for (auto fn : collectFunctions())
    runImpl(fn);
}

void RaiseArray::runImpl(FuncOp *func) {
  if (func->has<RecursiveAttr>()) {
    raiseRecursive(func);
    return;
  }

  Builder builder;
  int i = 0;
  for_all(AllocaOp, func) {
    auto dim = op->get<DimAttr>();
    if (!dim)
      continue;

    auto ty = op->ret()->type->pointee();
    builder.setToStart(module->getRegion());
    auto global = builder.create<GlobalOp>(ty);
    global->set<DimAttr>(*dim);
    global->name = "__arr_" + func->name + "_" + std::to_string(i++);

    if (ty == i32)
      global->set<ConstIArrAttr>(std::vector<int>(dim->size()));
    else {
      assert(ty == f32);
      global->set<ConstFArrAttr>(std::vector<float>(dim->size()));
    }

    builder.replace<GetGlobalOp>(op, Type::pointer(ty))->with(global->ret());
  }
}

// The basic idea is to emulate a stack of 1MB with arrays.
void RaiseArray::raiseRecursive(FuncOp *func) {
  Builder builder;
  int i = 0;
  auto allocas = collectOps<AllocaOp>(func);
  if (allocas.empty())
    return;

  // We need to add a `depth` argument to the function.
  // This is used to index the emulated stack.

  // For all calls to `func` inside the same recursive cycle, we need to increase depth by 1.
  // All of them now need a new depth argument.
  // For calls between them, the depth argument is passed as-is.
  std::set<FuncOp*> scc = tarjan->getSccContaining(func);
  std::map<FuncOp*, Value*> depths;
  for (auto fn : scc)
    depths[fn] = fn->pushResult(i32);

  for (auto fn : scc) {
    for_all(CallOp, fn) {
      if (!scc.count(cast<FuncOp>(op->val(0)->def)))
        continue;

      op->pushOperand(depths[fn]);
    }
  }
  
  auto depth = depths[func];

  // For all other functions, start with a depth of 0 when it calls any of these functions.
  for (auto fn : collectFunctions()) {
    if (scc.count(fn))
      continue;

    for_all(CallOp, fn) {
      if (!scc.count(cast<FuncOp>(op->val(0)->def)))
        continue;

      builder.setBefore(op);
      auto zero = builder.create<IntOp>(i32);
      op->pushOperand(zero->ret());
    }
  }

  for (auto op : allocas) {
    auto dim = op->get<DimAttr>();
    if (!dim)
      continue;

    // Emulate an 1MB stack with a global array.
    auto size = dim->size();
    auto firstDim = (1048576 + size - 1) / size;
    std::vector<int> dims { firstDim };
    data::concat(dims, dim->dims);
    size *= firstDim;

    auto ty = op->ret()->type->pointee();
    builder.setToStart(module->getRegion());
    auto global = builder.create<GlobalOp>(ty);
    global->set<DimAttr>(dims);
    global->name = "__arr_" + func->name + "_" + std::to_string(i++);

    if (ty == i32)
      global->set<ConstIArrAttr>(std::vector<int>(size));
    else {
      assert(ty == f32);
      global->set<ConstFArrAttr>(std::vector<float>(size));
    }

    builder.setBefore(op);
    auto get = builder.replace<GetGlobalOp>(op, Type::pointer(ty))->with(global->ret());

    // All references to the alloca should be replaced with `%get[%depth]`.
    for (auto use : get->ret()->getUses()) {
      assert(isa<ArrayLoadOp>(use) || isa<ArrayStoreOp>(use));
      use->insertOperand(1, depth);
    }
  }
}

}
