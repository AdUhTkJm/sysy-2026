#include "Printer.h"
#include "Ops.h"
#include "Attrs.h"
#include "../utils/DataStructure.h"
#include "../opt/low/Regs.h"
#include "../main/Options.h"
#include <cstring>

namespace ir {

Printer printer;

void printWithFormat(std::ostream &os, const Op *op, Printer *printer, const char *fmt) {
  for (const char *p = fmt; *p;) {
    if (*p != '$') {
      os << *p++;
      continue;
    }

    p++;
    switch (*p++) {
    // Operands.
    case 'x': {
      if (*p == '>') {
        p++;
        printer->printOperands(op, *p++ - '0');
        break;
      }
      int index = *p++ - '0';
      if (*p == '?') {
        p++;
        if (op->getNumOperands() <= (unsigned) index)
          break;
      }
      os << printer->str(op->val(index));
      break;
    }
    // Results.
    case 'r':
    case 'R': {
      if (*p == '>') {
        p++;
        printer->printResults(op, *p++ - '0');
        // The `=` is unnecessary when the result is empty.
        if (strncmp(p, " = ", 3) == 0 && op->getNumResults() == 0)
          p += 3;
        break;
      }
      Value *v = op->ret(*p++ - '0');
      os << printer->str(v);
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

void Printer::printResults(const Op *op, unsigned from) {
  for (size_t i = from; i < op->getNumResults(); i++) {
    os << str(op->ret(i));
    if (i != op->getNumResults() - 1)
      os << ", ";
  }
}

void Printer::printOperands(const Op *op, unsigned from) {
  for (size_t i = from; i < op->getNumOperands(); i++) {
    os << str(op->val(i));
    if (i != op->getNumOperands() - 1)
      os << ", ";
  }
}

void Printer::printTypeTo(const Type *type, std::ostream &os) {
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
  case Type::unit:
    os << "unit";
    break;
  case Type::ptr:
    printTypeTo(type->pointee(), os);
    os << "*";
    break;
  case Type::fn: {
    os << "(";
    const auto &args = type->argTypes();
    for (auto [i, x] : data::enumerate(args)) {
      printTypeTo(x, os);
      if (i != args.size())
        os << ", ";
    }
    os << ")";
    printTypeTo(type->retType(), os);
  }
  }
}

void Printer::printType(const Type *type) {
  printTypeTo(type, os);
}

#define printer(Ty) void print##Ty(std::ostream &os, const Op *op, Printer *printer)
#define attr_printer(Ty) void print##Ty(std::ostream &os, const Attr *attr, [[gnu::unused]] Printer *printer)

#define format(Ty, string) printer(Ty) { \
  printWithFormat(os, op, printer, string);\
}

format(AddIOp, "$r0 = $x0 + $x1");
format(AddLOp, "$r0 = $x0 + $x1: i64");
format(AddFOp, "$r0 = $x0 + $x1: f32");
format(SubIOp, "$r0 = $x0 - $x1");
format(SubFOp, "$r0 = $x0 - $x1: f32");
format(MulIOp, "$r0 = $x0 * $x1");
format(MulFOp, "$r0 = $x0 * $x1: f32");
format(DivIOp, "$r0 = $x0 / $x1");
format(DivFOp, "$r0 = $x0 / $x1: f32");
format(ModIOp, "$r0 = $x0 % $x1");
format(AndIOp, "$r0 = $x0 & $x1");
format(OrIOp , "$r0 = $x0 | $x1");
format(XorIOp, "$r0 = $x0 ^ $x1");
format(EqOp, "$r0 = $x0 == $x1");
format(NeOp, "$r0 = $x0 != $x1");
format(LtOp, "$r0 = $x0 < $x1");
format(LeOp, "$r0 = $x0 <= $x1");
format(NotOp, "$r0 = ! $x0");
format(ReturnOp, "return $x0?");
format(ModuleOp, "module");
format(IfOp, "$r>0 = if $x0");
format(WhileOp, "$r>0 = while");
format(ForOp, "$r>1 = for $r0 in range($x0, $x1, $x2)");
format(LoadOp, "$r0 = load $x0");
format(StoreOp, "store [$x0], $x1");
format(CallOp, "$r0 = $x0($x>1)");
format(GetGlobalOp, "$r0 = la $x0");
format(GlobalArrayOp, "$r0: $tr0");
format(YieldOp, "yield $x>0");
format(ConditionOp, "condition($x0); $x>1");
format(I2FOp, "$r0 = (f32) $x0");
format(F2IOp, "$r0 = (i32) $x0");
format(UndefOp, "$r0 = undef $tr0");
format(DoWhileOp, "$r>0 = do-while $x>0");
/* ARM operations */
format(AddWOp, "add $r0, $x0, $x1");
format(AddXOp, "add $R0, $X0, $X1");
format(SubWOp, "sub $r0, $x0, $x1");
format(SubXOp, "sub $R0, $X0, $X1");
format(MulWOp, "mul $r0, $x0, $x1");
format(MulXOp, "mul $R0, $X0, $X1");
format(DivWOp, "div $r0, $x0, $x1");
format(DivXOp, "div $R0, $X0, $X1");
format(CmpEqOp, "cmp.eq $r0, $x0, $x1");
format(CmpNeOp, "cmp.ne $r0, $x0, $x1");
format(CmpLtOp, "cmp.lt $r0, $x0, $x1");
format(CmpLeOp, "cmp.le $r0, $x0, $x1");
format(RetOp, "ret");

printer(AddWIOp) {
  auto addwi = cast<AddWIOp>(op);
  os << "add " << printer->str(op->ret()) << ", " << printer->str(op->val()) << ", #" << addwi->value;
}

printer(AddXIOp) {
  auto addwi = cast<AddXIOp>(op);
  os << "add " << printer->str(op->ret()) << ", " << printer->str(op->val()) << ", #" << addwi->value;
}

printer(MovIOp) {
  auto movi = cast<MovIOp>(op);
  os << "mov " << printer->str(op->ret()) << ", #" << movi->value;
}

printer(LdrOp) {
  auto ldr = cast<LdrOp>(op);
  os << "ldr " << printer->str(op->ret()) << ", [" << printer->str(op->val()) << ", #" << ldr->value << "]";
}

printer(StrOp) {
  auto str = cast<StrOp>(op);
  os << "str " << printer->str(op->val(1)) << ", [" << printer->str(op->val(0)) << ", #" << str->value << "]";
}

printer(LdpOp) {
  auto ldr = cast<LdpOp>(op);
  os << "ldp ";
  printer->printResults(ldr);
  os << ", [" << printer->str(op->val()) << ", #" << ldr->value << "]";
}

printer(StpOp) {
  auto str = cast<StpOp>(op);
  os << "stp ";
  printer->printOperands(op, 1);
  os << ", [" << printer->str(op->val(0)) << ", #" << str->value << "]";
}

printer(AdrpOp) {
  auto adrp = cast<AdrpOp>(op);
  os << "adrp " << printer->str(op->ret()) << ", " << adrp->name;
}

printer(AddXPOp) {
  auto addxp = cast<AddXPOp>(op);
  os << "add " << printer->str(op->ret()) << ", " << printer->str(op->val()) << ", :lo12:" << addxp->name;
}

printer(BOp) {
  auto jmp = cast<BOp>(op);
  os << "b " << printer->str(jmp->target);
}

printer(BlOp) {
  (void) printer;
  auto call = cast<BlOp>(op);
  os << "bl " << call->name;
  if (op->getNumOperands() > 0) {
    os << "(";
    printer->printOperands(op);
    os << ")";
  }
}

#define arm_branch_printer(Ty) \
  printer(Ty) { \
    auto br = cast<Ty>(op); \
    std::string x = #Ty; \
    x[0] = tolower(x[0]); \
    x = x.substr(0, x.size() - 2); /* Remove the final 'Op' */ \
    os << x << ' '; \
    printer->printOperands(op); \
    os << ", " << printer->str(br->target); \
    if (br->other) \
      os << ", " << printer->str(br->other); \
  }

arm_branch_op_list(arm_branch_printer)

printer(ExternCallOp) {
  auto extc = cast<ExternCallOp>(op);
  printer->printResults(op);
  if (extc->getNumResults() > 0)
    os << " = ";
  os << extc->name << "(";
  printer->printOperands(op);
  os << ')';
}

printer(GlobalOp) {
  auto glob = cast<GlobalOp>(op);
  os << printer->str(glob->ret()) << " = " << glob->name << ": ";
  printer->printType(glob->ret()->type);
}

printer(AllocaOp) {
  auto ret = op->ret();
  os << printer->str(ret) << " = alloca ";
  printer->printType(ret->type->pointee());
}

printer(ArrayStoreOp) {
  os << "store " << printer->str(op->val(0));
  for (unsigned i = 1; i < op->getNumOperands() - 1; i++)
    os << "[" << printer->str(op->val(i)) << ']';
  os << ", " << printer->str(op->getOperands().back());
}

printer(ArrayLoadOp) {
  os << printer->str(op->ret()) << " = " << printer->str(op->val(0));
  for (unsigned i = 1; i < op->getNumOperands(); i++)
    os << "[" << printer->str(op->val(i)) << ']';
}

printer(FuncOp) {
  auto fn = cast<FuncOp>(op);
  os << "func " << fn->name << " = ";
  // Print the function handle.
  os << printer->str(fn->getHandle());
  // Print argument list.
  os << "(";
  printer->printResults(fn, 1);
  os << ")";
}

printer(IntOp) {
  os << printer->str(op->ret(0)) << " = " << cast<IntOp>(op)->value;
}

printer(FloatOp) {
  os << printer->str(op->ret(0)) << " = " << cast<FloatOp>(op)->value;
}

printer(JumpOp) {
  auto jmp = cast<JumpOp>(op);
  os << "=> " << printer->str(jmp->target);
  if (jmp->getNumOperands() > 0) {
    os << "(";
    printer->printOperands(jmp);
    os << ")";
  }
}

printer(BranchOp) {
  auto br = cast<BranchOp>(op);
  os << "=> " << printer->str(op->val(0)) << " ? " << printer->str(br->target) << " : " << printer->str(br->other);
  if (br->getNumOperands() > 1) {
    os << " with ";
    printer->printOperands(br, 1);
  }
}

printer(PhiOp) {
  auto phi = cast<PhiOp>(op);
  os << "phi ";
  for (auto [value, block] : *phi)
    os << "[ " << printer->str(value) << ", " << printer->str(block) << " ] ";
}

printer(WriteRegOp) {
  auto wr = cast<WriteRegOp>(op);
  auto val = wr->val();
  std::string name = opt::regname(wr->reg);
  if (val->type == i32)
    name[0] = 'w';

  auto valreg = printer->str(val);
  if (valreg == name) {
    printer->setNewline(false);
    return;
  }
  os << "mov " << name << ", " << valreg;
}

printer(ReadRegOp) {
  auto wr = cast<ReadRegOp>(op);
  auto ret = wr->ret();
  std::string name = opt::regname(wr->reg);
  if (ret->type == i32)
    name[0] = 'w';

  auto retreg = printer->str(ret);
  if (retreg == name) {
    printer->setNewline(false);
    return;
  }
  os << "mov " << retreg << ", " << name;
}

attr_printer(IntAttr) {
  os << "<" << cast<IntAttr>(attr)->i << ">";
}

attr_printer(SizeAttr) {
  os << "<size = " << cast<SizeAttr>(attr)->size << ">";
}

attr_printer(DimAttr) {
  os << "<dims = " << cast<DimAttr>(attr)->dims << ">";
}

attr_printer(ConstIArrAttr) {
  auto iarr = cast<ConstIArrAttr>(attr);
  os << "<arr = ";
  if (iarr->value.size() > 0)
    os << iarr->value[0];
  for (unsigned i = 1; i < iarr->value.size() - iarr->zeroSuffix; i++)
    os << ", " << iarr->value[i];
  if (iarr->zeroSuffix > 0)
    os << ", " << iarr->zeroSuffix << " x 0";
  os << ">"; 
}

attr_printer(ConstFArrAttr) {
  auto farr = cast<ConstFArrAttr>(attr);
  os << "<arr = ";
  if (farr->value.size() > 0)
    os << farr->value[0];
  for (unsigned i = 1; i < farr->value.size() - farr->zeroSuffix; i++)
    os << ", " << farr->value[i];
  if (farr->zeroSuffix > 0)
    os << ", " << farr->zeroSuffix << " x 0";
  os << ">"; 
}

attr_printer(ImpureAttr) {
  (void) attr;
  os << "<impure>";
}

#define op_map_entry(Ty) { Ty::id, print##Ty },
#define attr_map_entry(Ty) { Ty::identifier(), print##Ty },

Printer::PrintMap &Printer::dispatch() {
  static PrintMap map {
    complete_op_list(op_map_entry)
  };
  return map;
}
Printer::AttrPrintMap &Printer::attrDispatch() {
  static AttrPrintMap map {
    attr_list(attr_map_entry)
  };
  return map;
}

int Printer::id(const Block *block) {
  auto it = blockid.find(block);
  return it == blockid.end() ? blockid[block] = bid++ : it->second;
}

int Printer::id(const Value *value) {
  auto it = valueid.find(value);
  return it == valueid.end() ? valueid[value] = vid++ : it->second;
}

std::string Printer::str(const Value *value) {
  auto it = idents.find(value);
  std::string name = (it == idents.end()) ? "%" + std::to_string(id(value)) : it->second;
  if (options.printType) {
    std::stringstream ss;
    ss << "(" << name << ": ";
    printTypeTo(value->type, ss);
    ss << ")";
    return ss.str();
  }

  return name;
}

std::string Printer::str(const Block *block) {
  return bbPrefix + std::to_string(id(block));
}

void Printer::indent() {
  for (int i = 0; i < depth; i++)
    os << "  ";
}

void Printer::printImpl(const Block *bb, bool tag) {
  auto numArgs = bb->getNumArgs();
  if (tag || numArgs != 0 || bbPrefix != "bb") {
    depth--;
    indent();
    os << str(bb);
    if (numArgs > 0) {
      os << "(" << str(bb->arg(0));
      for (unsigned i = 1; i < numArgs; i++)
        os << ", " << str(bb->arg(i));
      os << ")";
    }
    os << ":\n";
    depth++;
  }

  for (auto op : bb->getOps())
    print(op);
}

void Printer::print(const Block *bb) {
  printImpl(bb, true);
}

void Printer::print(const Region *region) {
  depth++;
  os << " {\n";
  bool tag = region->getNumBlocks() != 1;
  for (auto bb : region->getBlocks())
    printImpl(bb, tag);
  
  depth--;
  indent();
  os << "}";
}

void Printer::print(const Op *op) {
  if (newline)
    indent();
  newline = true;
  auto printfn = dispatch()[op->id];
  assert(printfn);
  printfn(os, op, this);
  for (const auto &[_, attr] : op->getAttrs())
    print(attr);
  for (auto r : op->getRegions())
    print(r);
  if (newline)
    os << '\n';
}

void Printer::print(const Attr *attr) {
  os << ' ';
  auto printfn = attrDispatch()[attr->id];
  assert(printfn);
  printfn(os, attr, this);
}

void Printer::reset() {
  depth = bid = vid = 0;
  blockid.clear();
  valueid.clear();
}

void Printer::dump(std::ostream &out) {
  out << os.str();
  os.str("");
  os.clear();
}

std::ostream &operator<<(std::ostream &os, Op *op) {
  printer.print(op);
  printer.dump(os);
  return os;
}

std::ostream &operator<<(std::ostream &os, Block *bb) {
  printer.print(bb);
  printer.dump(os);
  return os;
}

std::ostream &operator<<(std::ostream &os, Region *region) {
  printer.print(region);
  printer.dump(os);
  return os;
}

}

