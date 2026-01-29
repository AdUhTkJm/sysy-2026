#include "Printer.h"
#include "Ops.h"
#include "Attrs.h"
#include "../utils/DataStructure.h"
#include <cstring>

namespace ir {

Printer printer(std::cerr);

void printWithFormat(std::ostream &os, Op *op, Printer *printer, const char *fmt) {
  for (const char *p = fmt; *p;) {
    if (*p != '$') {
      os << *p++;
      continue;
    }

    p++;
    switch (*p++) {
    // Operands.
    case 'x': {
      int index = *p++ - '0';
      if (*p == '?') {
        p++;
        if (op->getNumOperands() <= (unsigned) index)
          break;
      }
      os << '%' << printer->id(op->val(index));
      break;
    }
    // Results.
    case 'r': {
      if (*p == '>') {
        p++;
        printer->printResults(op, *p++ - '0');
        // The `=` is unnecessary when the result is empty.
        if (strncmp(p, " = ", 3) == 0 && op->getNumResults() == 0)
          p += 3;
        break;
      }
      Value *v = op->ret(*p++ - '0');
      os << '%' << printer->id(v);
      break;
    }
    case 't': {
      char dis = *p++;
      int index = *p++ - '0';
      assert(dis == 'r' || dis == 'x');
      Value *v = dis == 'r' ? op->ret(index) : op->val(index);
      printer->printType(v->type);
      break;
    }
    default:
      assert(false && "invalid format string");
    }
  }
}

void Printer::printResults(Op *op, unsigned from) {
  for (size_t i = from; i < op->getNumResults(); i++) {
    os << '%' << id(op->ret(i));
    if (i != op->getNumResults() - 1)
      os << ", ";
  }
}

void Printer::printOperands(Op *op,  unsigned from) {
  for (size_t i = from; i < op->getNumOperands(); i++) {
    os << '%' << id(op->val(i));
    if (i != op->getNumOperands() - 1)
      os << ", ";
  }
}

void Printer::printType(const Type *type) {
  switch (type->kind) {
  case Type::i32:
    os << "i32";
    break;
  case Type::i64:
    os << "i64";
    break;
  case Type::f32:
    os << "f32";
    break;
  case Type::vi4:
    os << "vi4";
    break;
  case Type::vf4:
    os << "vf4";
    break;
  case Type::ptr:
    printType(type->pointee());
    os << "*";
    break;
  case Type::fn: {
    os << "(";
    const auto &args = type->argTypes();
    for (auto [i, x] : data::enumerate(args)) {
      printType(x);
      if (i != args.size())
        os << ", ";
    }
    os << ")";
    printType(type->retType());
  }
  }
}

#define printer(Ty) void print##Ty(std::ostream &os, Op *op, Printer *printer)

#define format(Ty, string) printer(Ty) { \
  printWithFormat(os, op, printer, string);\
}

format(AddIOp, "$r0 = $x0 + $x1");
format(SubIOp, "$r0 = $x0 - $x1");
format(MulIOp, "$r0 = $x0 * $x1");
format(DivIOp, "$r0 = $x0 / $x1");
format(ModIOp, "$r0 = $x0 % $x1");
format(AndIOp, "$r0 = $x0 & $x1");
format(OrIOp , "$r0 = $x0 | $x1");
format(XorIOp, "$r0 = $x0 ^ $x1");
format(ReturnOp, "return $x0?");
format(ModuleOp, "module");
format(IfOp, "$r>0 = if $x0");
format(WhileOp, "$r>0 = while $x0");
format(ForOp, "$r>1 = for $r0 in range($x0, $x1, $x2)");
format(LoadOp, "$r0 = load $x0");
format(StoreOp, "store $x0, $x1");

printer(AllocaOp) {
  auto ret = op->ret();
  os << '%' << printer->id(ret) << " = alloca ";
  printer->printType(ret->type->pointee());
}

printer(FuncOp) {
  auto fn = cast<FuncOp>(op);
  os << "func " << fn->name << " = %";
  // Print the function handle.
  os << printer->id(fn->getHandle());
  // Print argument list.
  os << "(";
  printer->printResults(fn, 1);
  os << ")";
}

printer(IntOp) {
  os << '%' << printer->id(op->ret(0)) << " = " << cast<IntOp>(op)->value;
}

printer(FloatOp) {
  os << '%' << printer->id(op->ret(0)) << " = " << cast<FloatOp>(op)->value;
}

printer(JumpOp) {
  auto jmp = cast<JumpOp>(op);
  os << "=> bb" << printer->id(jmp->target);
  if (jmp->getNumResults() > 0) {
    os << " with ";
    printer->printOperands(jmp);
  }
}

printer(BranchOp) {
  auto br = cast<BranchOp>(op);
  os << "=> " << printer->id(op->val(0)) << " ? bb" << printer->id(br->target) << " : bb" << printer->id(br->other);
  if (br->getNumResults() > 0) {
    os << " with ";
    printer->printOperands(br, 1);
  }
}

printer(PhiOp) {
  auto phi = cast<PhiOp>(op);
  os << "phi ";
  for (auto [value, block] : *phi)
    os << "[ " << printer->id(value) << ", bb" << printer->id(block) << " ] ";
}

#define map_entry(Ty) { Ty::identifier(), print##Ty },

Printer::PrintMap &Printer::dispatch() {
  static PrintMap map {
    complete_op_list(map_entry)
  };
  return map;
}

int Printer::id(Block *block) {
  auto it = blockid.find(block);
  return it == blockid.end() ? blockid[block] = bid++ : it->second;
}

int Printer::id(Value *value) {
  auto it = valueid.find(value);
  return it == valueid.end() ? valueid[value] = vid++ : it->second;
}

void Printer::indent() {
  for (int i = 0; i < depth; i++)
    os << "  ";
}

void Printer::printImpl(Block *bb, bool tag) {
  if (tag) {
    depth--;
    indent();
    os << "bb" << id(bb) << ":\n";
    depth++;
  }

  for (auto op : bb->getOps())
    print(op);
}

void Printer::print(Block *bb) {
  printImpl(bb, true);
}

void Printer::print(Region *region) {
  depth++;
  os << " {\n";
  bool tag = region->getNumBlocks() != 1;
  for (auto bb : region->getBlocks())
    printImpl(bb, tag);
  
  depth--;
  indent();
  os << "}";
}

void Printer::print(Op *op) {
  indent();
  auto printfn = dispatch()[op->id];
  assert(printfn);
  printfn(os, op, this);
  for (auto r : op->getRegions())
    print(r);
  os << '\n';
}

void Printer::reset() {
  depth = bid = vid = 0;
  blockid.clear();
  valueid.clear();
}

std::ostream &operator<<(std::ostream &os, Op *op) {
  printer.print(op);
  return os;
}

std::ostream &operator<<(std::ostream &os, Block *bb) {
  printer.print(bb);
  return os;
}

std::ostream &operator<<(std::ostream &os, Region *region) {
  printer.print(region);
  return os;
}

}

