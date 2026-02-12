#include "Ops.h"

namespace ir {

#define int_float_operator_list(X) \
  X(+) X(-) X(*) X(/) X(==) X(!=) X(<=) X(<)

#define int_operator_list(X) \
  X(%) X(|) X(&) X(^) X(<<) X(>>)

#define runtime_value_operator_list(X) \
  int_float_operator_list(X) int_operator_list(X)

#define operator_decl(w) \
  RuntimeValue operator w(RuntimeValue other) const;

struct Pointer {
  unsigned id;
  unsigned offset;
};

// Runtime value.
class RuntimeValue {
public:
  union {
    int64_t vi;
    float vf;
    Pointer vp;
  };

  enum Kind {
    None,
    Int,
    Float,
    Ptr,
  } kind = None;

  RuntimeValue(): kind(None) {}
  RuntimeValue(int v): RuntimeValue((int64_t) v) {}
  RuntimeValue(int64_t v): vi(v), kind(Int) {}
  RuntimeValue(float v): vf(v), kind(Float) {}
  RuntimeValue(Pointer ptr): vp(ptr), kind(Ptr) {}

  bool compatible(RuntimeValue other) const { return kind == other.kind; }
  bool isInt() const { return kind == Int; }
  bool isFloat() const { return kind == Float; }
  bool isPtr() const { return kind == Ptr; }
  operator bool() const { assert(isInt()); return vi; }

  runtime_value_operator_list(operator_decl)
};

class Memory {
  struct Allocation {
    std::vector<unsigned char> bytes;
  };

  static unsigned nextId;
  std::unordered_map<unsigned, Allocation> allocations;
public:
  Pointer allocate(size_t size);
  bool has(Pointer ptr) const;
  void store(Pointer ptr, RuntimeValue val);
  RuntimeValue load(Pointer ptr) const;
};

struct Frame {
  using ValueMap = std::unordered_map<Value*, RuntimeValue>;

  FuncOp *func;
  Block *current, *pred;
  ValueMap values;
  Memory memory;
};

struct ExecResult {
  enum Kind {
    Continue,
    Yield,
    Return
  } kind = Continue;
  std::vector<RuntimeValue> values;
};

class Interpreter {
public:
  using Values = std::vector<RuntimeValue>;
private:
  std::vector<Frame> stack;
  Memory globalMemory;
  std::map<Value*, Pointer> globals;

  using OpHandler = ExecResult (*)(Interpreter&, Op*);
  static std::unordered_map<OpKind, OpHandler> handlers;
  
public:
  ExecResult executeFunction(FuncOp *fn, const Values &args);
  ExecResult execute(Block *block);
  ExecResult execute(Region *region);
  ExecResult execute(Op *op);

  Frame &frame() { return stack.back(); }

  ExecResult jumpTo(Block *bb, const Values &args);
  void setResults(Op *op, const Values &args);
  void resolvePhis(Block *bb);

  void addGlobal(Value *handle, unsigned size);
  Pointer getGlobal(Value *handle);

  RuntimeValue load(Pointer ptr);
  void store(Pointer ptr, RuntimeValue v);

  RuntimeValue &operator[](Value *v);
};

}

#undef operator_decl
