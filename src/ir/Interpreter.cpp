#include "Interpreter.h"
#include "../utils/DataStructure.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wreturn-type"

namespace opt {

int asmSize(const ir::AllocaOp *op);
int asmSize(const ir::Op *op);

}

namespace ir {

#define int_operator_defn(w) \
  RuntimeValue RuntimeValue::operator w(RuntimeValue other) const { \
    assert(isInt() && other.isInt()); \
    return RuntimeValue(vi w other.vi); \
  }

#define int_float_operator_defn(w) \
  RuntimeValue RuntimeValue::operator w(RuntimeValue other) const { \
    assert(compatible(other)); \
    if (isInt())\
      return RuntimeValue(vi w other.vi); \
    return RuntimeValue(vf w other.vf); \
  }

int_operator_list(int_operator_defn)
int_float_operator_list(int_float_operator_defn)

#define executor(Ty) \
  ExecResult execute##Ty(Interpreter &interp, Op *op)

#define executor_decl(Ty) \
  { OpKind::Ty, execute##Ty },

#define executor_binary(Ty, w) \
  executor(Ty) { \
    auto l = interp[op->val(0)], r = interp[op->val(1)]; \
    interp[op->ret()] = l w r; \
    return ExecResult { ExecResult::Continue, {} }; \
  }

#define executor_no_direct(Ty) \
  executor(Ty) { \
    (void) op; (void) interp; \
    assert(false && #Ty " should not be executed directly"); \
  }

#define binaries(X) \
  X(AddIOp, +) X(SubIOp, -) X(MulIOp, *) X(DivIOp, /) X(ModIOp, %) \
  X(AddLOp, +) X(SubLOp, -) X(MulLOp, *) X(DivLOp, /) X(ModLOp, %) \
  X(AddFOp, +) X(SubFOp, -) X(MulFOp, *) X(DivFOp, /) \
  X(AddWOp, +) X(SubWOp, -) X(MulWOp, *) X(DivWOp, /) \
  X(AddXOp, +) X(SubXOp, -) X(MulXOp, *) X(DivXOp, /) \
  X(AndIOp, &) X(OrIOp, |) X(XorIOp, ^) \
  X(EqOp, ==) X(NeOp, !=) X(LtOp, <) X(LeOp, <=) \
  X(CmpEqOp, ==) X(CmpNeOp, !=) X(CmpLtOp, <) X(CmpLeOp, <=)

#define executor_arm_branch(Ty, cond) \
  executor(Ty) { \
    auto br = cast<Ty>(op); \
    if (cond) \
      return interp.jumpTo(br->target, {}); \
    return interp.jumpTo(br->other, {}); \
  }

#define arm_branches(X) \
  X(CbzOp, interp[op->val(0)] == RuntimeValue(0)) \
  X(CbnzOp, interp[op->val(0)] != RuntimeValue(0)) \
  X(BeqOp, interp[op->val(0)] == interp[op->val(1)]) \
  X(BneOp, interp[op->val(0)] != interp[op->val(1)]) \
  X(BltOp, interp[op->val(0)] <  interp[op->val(1)]) \
  X(BgeOp, interp[op->val(0)] >= interp[op->val(1)]) \
  X(BleOp, interp[op->val(0)] <= interp[op->val(1)]) \
  X(BgtOp, interp[op->val(0)] >  interp[op->val(1)]) \

#define no_direct(X) \
  X(FuncOp) X(GlobalOp) X(PhiOp) X(GlobalArrayOp) \
  X(ForOp) /*TO BE REMOVED*/

binaries(executor_binary)
arm_branches(executor_arm_branch)
no_direct(executor_no_direct)

executor(ModuleOp) {
  auto bb = op->getRegion()->getFirstBlock();
  // Allocate globals.
  for (auto x : *bb) {
    if (auto g = dyn_cast<GlobalOp>(x)) {
      interp.addGlobal(g->ret(), opt::asmSize(g));
      continue;
    }
  }

  // Execute the main function in the module.
  for (auto x : *bb) {
    auto fn = dyn_cast<FuncOp>(x);
    if (!fn || fn->name != "main")
      continue;

    return interp.execute(fn);
  }

  assert(false && "module does not contain main!");
}

executor(ReturnOp) {
  if (op->getNumOperands() > 0)
    return { ExecResult::Return, { interp[op->val()] } };

  return { ExecResult::Return, {} };
}

executor(YieldOp) {
  std::vector<RuntimeValue> values;
  values.reserve(op->getNumOperands());

  for (auto x : op->getOperands())
    values.push_back(interp[x]);

  return { ExecResult::Yield, values };
}

executor(IfOp) {
  auto cond = interp[op->val(0)];
  assert(cond.isInt());

  auto region = cond.vi ? op->getRegion(0) : op->getRegion(1);
  auto result = interp.execute(region);
  assert(result.kind == ExecResult::Kind::Yield);

  interp.setResults(op, result.values);
  return {};
}

executor(DoWhileOp) {
  RuntimeValue cond;
  do {
    auto region = cond.vi ? op->getRegion(0) : op->getRegion(1);
    auto result = interp.execute(region);
    assert(result.kind == ExecResult::Kind::Yield);

    auto last = region->getLastOp();
    assert(isa<ConditionOp>(last));

    cond = interp[last->val()];
    assert(cond.isInt());

    interp.setResults(op, result.values);
  } while (cond.vi != 0);
  return {};
}

executor(LoadOp) {
  auto ptr = interp[op->val()];
  assert(ptr.isPtr());
  interp[op->ret()] = interp.load(ptr.vp);
  return {};
}

executor(StoreOp) {
  auto ptr = interp[op->val()];
  assert(ptr.isPtr());
  interp.store(ptr.vp, interp[op->val(1)]);
  return {};
}

executor(ArrayLoadOp) {
  auto ptr = interp[op->val()];
  assert(ptr.isPtr());
  // TODO: add offset
  interp[op->ret()] = interp.load(ptr.vp);
  assert(false && "no array load yet");
  return {};
}

executor(ArrayStoreOp) {
  auto ptr = interp[op->val()];
  assert(ptr.isPtr());
  // TODO: add offset
  interp.store(ptr.vp, interp[op->getResults().back()]);
  assert(false && "no array store yet");
  return {};
}

executor(AllocaOp) {
  auto size = opt::asmSize(cast<AllocaOp>(op));
  auto ptr = interp.frame().memory.allocate(size);
  interp[op->ret()] = ptr;
  return {};
}

executor(CallOp) {
  auto fn = cast<FuncOp>(op->val()->def);
  
  Interpreter::Values args;
  args.reserve(op->getNumOperands() - 1);
  for (unsigned i = 1; i < op->getNumOperands(); i++)
    args.push_back(interp[op->val(i)]);

  interp.executeFunction(fn, args);
  return {};
}

executor(NotOp) {
  interp[op->ret()] = ! (bool) interp[op->val()];
  return {};
}

executor(UndefOp) {
  interp[op->ret()] = RuntimeValue();
  return {};
}

executor(I2FOp) {
  auto v = interp[op->val()];
  assert(v.isInt());
  interp[op->ret()] = RuntimeValue((float) v.vi);
  return {};
}

executor(F2IOp) {
  auto v = interp[op->val()];
  assert(v.isFloat());
  interp[op->ret()] = RuntimeValue((int) v.vf);
  return {};
}

executor(ConditionOp) {
  // Behaves exactly the same as yield, but with one less argument.
  std::vector<RuntimeValue> values;
  values.reserve(op->getNumOperands() - 1);

  for (unsigned i = 1; i < op->getNumOperands(); i++)
    values.push_back(interp[op->val(i)]);

  return { ExecResult::Yield, values };
}

executor(GetGlobalOp) {
  interp[op->ret()] = interp.getGlobal(op->val());
  return {};
}

executor(IntOp) {
  interp[op->ret()] = RuntimeValue(cast<IntOp>(op)->value);
  return {};
}

executor(FloatOp) {
  interp[op->ret()] = RuntimeValue(cast<FloatOp>(op)->value);
  return {};
}

executor(BranchOp) {
  auto cond = interp[op->val()];
  auto br = cast<BranchOp>(op);

  std::vector<RuntimeValue> args;
  args.reserve(op->getNumOperands() - 1);

  for (unsigned i = 1; i < op->getNumOperands(); i++)
    args.push_back(interp[op->val(i)]);

  if (cond)
    return interp.jumpTo(br->target, args);
  return interp.jumpTo(br->other, args);
}

executor(JumpOp) {
  auto j = cast<JumpOp>(op);

  std::vector<RuntimeValue> args;
  args.reserve(op->getNumOperands());

  for (unsigned i = 0; i < op->getNumOperands(); i++)
    args.push_back(interp[op->val(i)]);

  return interp.jumpTo(j->target, args);
}

/* ARM operations. */
executor(ExternCallOp) {

}

executor(RetOp) {

}

executor(BOp) {
  
}

executor(BlOp) {
  
}

executor(MovIOp) {
  
}

executor(AdrpOp) {
  
}

executor(AddXPOp) {
  
}

executor(LdrOp) {
  
}

executor(LdpOp) {
  
}

executor(StrOp) {
  
}

executor(StpOp) {
  
}

executor(WriteRegOp) {
  
}

executor(ReadRegOp) {

}

executor(AddWIOp) {

}

executor(AddXIOp) {
  
}

std::unordered_map<OpKind, Interpreter::OpHandler> Interpreter::handlers {
  // complete_op_list(executor_decl)
};

RuntimeValue &Interpreter::operator[](Value *v) {
  auto &frame = stack.back();
  assert(frame.values.count(v) && "undefined SSA value");
  return frame.values[v];
}

void Interpreter::setResults(Op *op, const Values &args) {
  auto &frame = stack.back();
  for (auto [i, ret] : data::enumerate(op->getResults()))
    frame.values[ret] = args[i];
}

void Interpreter::resolvePhis(Block* bb) {
  auto &frame = stack.back();
  std::vector<std::pair<Value*, RuntimeValue>> updates;

  for (auto op : *bb) {
    auto phi = dyn_cast<PhiOp>(op);
    if (!phi)
      break;

    Value *incoming = phi->incomingFrom(frame.pred);
    RuntimeValue val = frame.values[incoming];
    updates.emplace_back(op->ret(0), val);
  }
  for (auto& [result, val] : updates)
    frame.values[result] = val;
}

ExecResult Interpreter::jumpTo(Block* bb, const Values &args) {
  auto &frame = stack.back();
  frame.pred = frame.current;
  frame.current = bb;
  for (size_t i = 0; i < args.size(); ++i)
    frame.values[bb->getArgs()[i]] = args[i];

  return execute(bb);
}

RuntimeValue Interpreter::load(Pointer ptr) {
  if (globalMemory.has(ptr))
    return globalMemory.load(ptr);

  return frame().memory.load(ptr);
}

void Interpreter::store(Pointer ptr, RuntimeValue v) {
  if (globalMemory.has(ptr)) {
    globalMemory.store(ptr, v);
    return;
  }

  frame().memory.store(ptr, v);
}

void Interpreter::addGlobal(Value *handle, unsigned size) {
  auto ptr = globalMemory.allocate(size);
  globals[handle] = ptr;
}

Pointer Interpreter::getGlobal(Value *handle) {
  assert(globals.count(handle));
  return globals[handle];
}

ExecResult Interpreter::execute(Op *op) {
  return handlers[(OpKind) op->id](*this, op);
}

ExecResult Interpreter::execute(Block *bb) {
  for (auto op : *bb) {
    if (auto result = execute(op); result.kind != ExecResult::Continue)
      return result;
  }

  return {};
}

ExecResult Interpreter::execute(Region *region) {
  return execute(region->getFirstBlock());
}

ExecResult Interpreter::executeFunction(FuncOp *fn, const Values &args) {
  stack.push_back(Frame{});
  auto &frame = stack.back();

  auto region = fn->getRegion();
  auto bb = region->getFirstBlock();
  frame.current = bb;
  frame.pred = nullptr;
  frame.func = fn;
  for (auto [i, x] : data::enumerate(args))
    frame.values[fn->getArg(i)] = x;
}

unsigned Memory::nextId = 1;

Pointer Memory::allocate(size_t size) {
  unsigned id = nextId++;
  allocations[id] = Allocation { std::vector<unsigned char>(size) };
  return Pointer { id, 0 };
}

void Memory::store(Pointer ptr, RuntimeValue val) {
  allocations.at(ptr.id).bytes[ptr.offset] = val;
}

bool Memory::has(Pointer ptr) const {
  return allocations.count(ptr.id);
}

RuntimeValue Memory::load(Pointer ptr) const {
  return allocations.at(ptr.id).bytes[ptr.offset];
}

}
