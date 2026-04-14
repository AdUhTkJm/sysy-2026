#include "Common.h"
#include <cstring>
#include <algorithm>

namespace opt {

#define allowed_list(X) \
  X(IntOp) X(FloatOp) X(GetGlobalOp) \
  X(AddIOp) X(SubIOp) X(MulIOp) X(DivIOp) X(ModIOp) \
  X(AddLOp) X(SubLOp) X(MulLOp) X(DivLOp) \
  X(AddFOp) X(SubFOp) X(MulFOp) X(DivFOp) \
  X(LtOp) X(LeOp) X(EqOp) X(NeOp) \
  X(LShiftOp) X(RShiftOp) X(MinOp) X(MaxOp) X(SelectOp) \
  X(EqFOp) X(NeFOp) X(LtFOp) X(LeFOp) X(NotFOp) X(SextOp)

#define allow(Ty) \
  isa<Ty>(op) ||

static bool allowed(Op *op) {
  return allowed_list(allow) (isa<CallOp>(op) && !op->has<UnerasableAttr>());
}

Pass *makeHighDCE(ModuleOp *module);
Pass *makePure(ModuleOp *module);

struct Key {
  int id;
  int loc;
  std::vector<int> operands;
  union {
    int vi;
    float vf;
  };

  bool operator<(const Key &other) const {
    if (auto diff = id - other.id)
      return diff < 0;
    if (auto diff = loc - other.loc)
      return diff < 0;
    if (auto diff = (int) operands.size() - (int) other.operands.size())
      return diff < 0;
    for (unsigned i = 0; i < operands.size(); i++) {
      if (operands[i] != other.operands[i])
        return operands[i] < other.operands[i];
    }

    if (id == IntOp::id)
      return vi < other.vi;
    if (id == FloatOp::id)
      return vf < other.vf;

    return false; // Equal.
  }
};

using ExprTable = std::map<Key, int>;
using ValueTable = std::map<int, Value*>;
using SymbolTable = std::map<Value*, int>;

declare_pass(HighGVN,
  void walk(Op *parent);
  Key hash(Value *v) const;

  ExprTable exprNum;
  SymbolTable symbols;
  ValueTable numOp;
  int index = 1;

  class SemanticScope {
    HighGVN &pass;
    ExprTable exprNum;
    SymbolTable symbols;
    ValueTable numVal;
    bool invalidated = false;
  public:
    SemanticScope(HighGVN &pass):
      pass(pass), exprNum(pass.exprNum), symbols(pass.symbols), numVal(pass.numOp) {}
    ~SemanticScope() {
      if (!invalidated) {
        pass.symbols = symbols;
        pass.exprNum = exprNum;
        pass.numOp = numVal;
      }
    }
    void invalidate() { invalidated = true; }
  };
) {
  makePure(module)->run();

  // Calls and get_globals refer to globally declared values.
  // We first add them into our tables.
  for (auto top : *module->getRegion()->getFirstBlock()) {
    Value *v = top->ret(0);
    Key k = hash(v);
    symbols[v] = exprNum[k] = index;
    numOp[index] = v;
    index++;
  }

  for (auto func : collectFunctions()) {
    SemanticScope scope(*this);
    // Add function arguments.
    for (unsigned i = 1; i < func->getNumResults(); i++) {
      Value *v = func->ret(i);
      Key k = hash(v);
      symbols[v] = exprNum[k] = index;
      numOp[index] = v;
      index++;
    }
    walk(func);
  }

  makeHighDCE(module)->run();
};

Key HighGVN::hash(Value *v) const {
  auto op = v->def;
  Key k;
  k.id = op->id;
  k.loc = op->getOperandIndex(v);

  for (auto [i, x] : data::enumerate(op->getOperands())) {
    if (!symbols.count(x)) {
      std::cerr << "domination violated: " << v << "\n";
      assert(false && "bad symbol");
    }
    k.operands.push_back(symbols.at(x));
  }
  if (isa<AddIOp>(op) || isa<MulIOp>(op))
    std::sort(k.operands.begin(), k.operands.end());

  if (auto i = dyn_cast<IntOp>(op))
    k.vi = i->value;
  if (auto f = dyn_cast<FloatOp>(op))
    k.vf = f->value;

  return k;
}

void HighGVN::walk(Op *op) {
  if (allowed(op)) {
    auto v = op->ret();
    auto k = hash(v);
    if (auto it = exprNum.find(k); it != exprNum.end()) {
      v->replaceAllUsesWith(numOp[it->second]);
    } else goto x;
  } else if (!isa<FuncOp>(op)) x: for (auto ret : op->getResults()) {
    symbols[ret] = exprNum[hash(ret)] = index;
    numOp[index] = ret;
    index++;
  }

  for (auto r : op->getRegions()) {
    SemanticScope scope(*this);
    for (auto bb : *r) {
      for (auto x : *bb)
        walk(x);
    }
  }
}

}
