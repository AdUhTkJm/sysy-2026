#ifndef PASS_H
#define PASS_H

#include "../ir/Ops.h"
#include "../main/Options.h"

namespace opt {

class Pass {
protected:
  ir::ModuleOp *const module;
  std::vector<ir::FuncOp*> collectFunctions();

  template<class T>
  std::vector<T*> collectOps(ir::Op *root) {
    std::vector<T*> result;
    walk(root, [&](ir::Op *op) {
      if (auto x = dyn_cast<T>(op))
        result.push_back(x);
    });
    return result;
  }

  template<class T>
  std::vector<T*> collectOps() { return collectOps<T>(module); }

  template<class F> __requires((std::invocable<F, ir::Op*>))
  void walk(ir::Op *parent, const F &f) {
    f(parent);
    for (auto r : parent->getRegions()) {
      for (auto bb : *r) {
        for (auto op : *bb)
          walk(op, f);
      }
    }
  }
public:
  virtual void run() = 0;
  virtual const char *name() = 0;
  Pass(ir::ModuleOp *module): module(module) {}
};

class PassManager {
  std::vector<Pass*> passes;
  const Options options;
public:
  ir::ModuleOp *const module;
  
  void run();
  void addPass(Pass *pass);
  PassManager(ir::ModuleOp *module, const Options &options): options(options), module(module) {}
};

#define make_pass(Ty) Pass *make##Ty(ir::ModuleOp *module)
#define make_pass_impl(Ty) make_pass(Ty) { return new Ty(module); }
#define pass_skeleton(Ty, ...) \
  class Ty : public Pass { \
    __VA_ARGS__ \
  public: \
    void run(); \
    const char *name() { return #Ty; } \
    Ty(ir::ModuleOp *module): Pass(module) {} \
  }; \

#define declare_pass(Ty, ...) \
  pass_skeleton(Ty, __VA_ARGS__) \
  make_pass_impl(Ty) \
  void Ty::run()

// For local (intra-procedure) passees.
#define declare_local_pass(Ty, ...) \
  pass_skeleton(Ty, __VA_ARGS__ void runImpl(FuncOp *func);) \
  make_pass_impl(Ty) \
  void Ty::run() { for (auto x : collectFunctions()) runImpl(x); } \
  void Ty::runImpl(FuncOp *func)

#define add_pass(Ty) \
  pm.addPass(opt::make##Ty(pm.module))

#define fixed(...) \
  { bool __changed; do { __changed = false; __VA_ARGS__ } while (__changed); }

#define mark_changed \
  __changed = true

#define for_all(Ty, x) \
  for (auto op : collectOps<Ty>(x))
}

#endif
