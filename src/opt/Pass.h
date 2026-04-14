#ifndef PASS_H
#define PASS_H

#include "../ir/Ops.h"

namespace opt {

class Preorder {};
class Postorder {};

#if __cplusplus >= 202002L
template<class T>
concept walk_order = std::same_as<T, Preorder> || std::same_as<T, Postorder>;
#endif

class Pass {
private:
  template<class F>
  static void walkImpl(ir::Op *parent, const F &f, Preorder) {
    f(parent);
    for (auto r : parent->getRegions()) {
      for (auto bb : *r) {
        for (auto it = bb->begin(); it != bb->end();) {
          auto next = it; next++;
          walk(*it, f);
          it = next;
        }
      }
    }
  }

  template<class F>
  static void walkImpl(ir::Op *parent, const F &f, Postorder) {
    for (auto r : parent->getRegions()) {
      for (auto bb : *r) {
        for (auto it = bb->begin(); it != bb->end();) {
          auto next = it; next++;
          walk(*it, f);
          it = next;
        }
      }
    }
    f(parent);
  }

  template<class T, class F> __requires(walk_order<T>)
  static void walkImpl(ir::Op *parent, const F &f) {
    walkImpl(parent, f, T{});
  }
public:
  using CallGraph = std::map<ir::FuncOp*, std::set<ir::FuncOp*>>;
protected:
  ir::ModuleOp *const module;
  std::vector<ir::FuncOp*> collectFunctions() const;

  template<class T>
  std::vector<T*> collectOps(ir::Op *root) const {
    std::vector<T*> result;
    walk(root, [&](ir::Op *op) {
      if (auto x = dyn_cast<T>(op))
        result.push_back(x);
    });
    return result;
  }

  template<>
  std::vector<ir::Op*> collectOps(ir::Op *root) const {
    std::vector<ir::Op*> result;
    walk(root, [&](ir::Op *op) {
      result.push_back(op);
    });
    return result;
  }

  template<class T>
  // Note: this only traverses a single layer.
  std::vector<T*> collectOps(ir::Block *bb) const {
    std::vector<T*> result;
    for (auto op : *bb) {
      if (auto x = dyn_cast<T>(op))
        result.push_back(x);
    }
    return result;
  }

  template<class T>
  std::vector<T*> collectOps() const { return collectOps<T>(module); }

  template<class T> __requires((std::derived_from<T, ir::Op>))
  bool contains(ir::Op *op) {
    if (isa<T>(op))
      return true;

    for (auto r : op->getRegions()) {
      for (auto bb : *r) {
        for (auto x : *bb) {
          if (contains<T>(x))
            return true;
        }
      }
    }
    return false;
  }

  ir::GlobalOp *findGlobal(std::string_view name) const;
  ir::FuncOp *findFunction(std::string_view name) const;

  // Returns the base of the address that the operation accesses.
  // This can be either a function argument, a GlobalOp or an alloca.
  ir::Op *directBaseOf(ir::Op *op) const;
  ir::Op *baseOf(ir::Value *v) const;

  std::set<ir::Value*> getVariantsIn(ir::DoWhileOp *loop) const;

  // u -> v means `u` is called by `v`.
  CallGraph calledGraph() const;
  // u -> v means `u` calls `v`.
  CallGraph callGraph() const;

  // Retrieves the main function.
  ir::FuncOp *findMain() const { return findFunction("main"); }

  // Finds the loop increment if present.
  ir::Value *increment(ir::Value *indvar) const;
  // Finds the loop variable tested in the loop condition, along with its limit.
  ir::Value *indvar(ir::DoWhileOp *loop, ir::Value **limit = nullptr) const;

  void moveChainBefore(ir::Op *op, ir::Op *anchor, ir::Op *loop) const;

  void checkAssignmentLegality(ir::Op *parent) const;
public:
  virtual void run() = 0;
  virtual const char *name() = 0;
  Pass(ir::ModuleOp *module): module(module) {}

  template<class T = Preorder, class F> __requires((std::invocable<F, ir::Op*>) && walk_order<T>)
  static void walk(ir::Op *parent, const F &f) {
    walkImpl<T>(parent, f);
  }
};

class PassManager {
  std::vector<Pass*> passes;
  std::map<const char *, int> time;
  
  bool shouldPrintBefore(const char *name) const;
  bool shouldPrintAfter(const char *name) const;
public:
  ir::ModuleOp *const module;
  
  void run();
  void addPass(Pass *pass);
  PassManager(ir::ModuleOp *module): module(module) {}
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
  { \
    bool __changed; int __index = 0; \
    do { __changed = false; if (__index > 100) assert(false && "fixed does not converge"); __VA_ARGS__ } \
    while (++__index, __changed); \
  }

#define mark_changed \
  __changed = true

#define mark_changed_if(...) \
  __changed |= (__VA_ARGS__)

#define for_all(Ty, ...) \
  for (auto op : collectOps<Ty>(__VA_ARGS__))

#define for_ops_in(parent) \
  for (auto r : parent->getRegions()) \
    for (auto bb : *r) \
      for (auto op : *bb) \

}

#endif
