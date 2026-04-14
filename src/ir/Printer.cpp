#include "Printer.h"
#include "Ops.h"
#include "Attrs.h"
#include "Regs.h"
#include "../utils/DataStructure.h"
#include "../main/Options.h"
#include "../opt/high/Common.h"
#include <cstring>

namespace ir {

Printer printer;

void printWithFormat(std::ostream &os, const Op *op, Printer *printer, const char *fmt) {
  // The '\\' at front means this is a hidden operation.
  if (*fmt == '\\') {
    if (!printer->showHidden)
      return;

    fmt++;
  }

  for (const char *p = fmt; *p;) {
    if (*p != '$') {
      auto c = *p++;
      if (c == '\n') {
        printer->printNewline(os);
        continue;
      }

      os << c;
      continue;
    }

    p++;
    auto c = *p++;
    switch (c) {
    // Operands.
    case 'x':
    case 'X': {
      if (*p == '>') {
        p++;
        printer->printOperands(os, op, *p++ - '0');
        break;
      }
      int index = *p++ - '0';
      if (*p == '?') {
        p++;
        if (op->getNumOperands() <= (unsigned) index)
          break;
      }
      auto str = printer->str(op->val(index));
      if (c == 'X' && str[0] == 'w')
        str[0] = 'x';
      os << str;
      break;
    }
    // Results.
    case 'r':
    case 'R': {
      if (*p == '>') {
        p++;
        printer->printResults(os, op, *p++ - '0');
        // The `=` is unnecessary when the result is empty.
        if (strncmp(p, " = ", 3) == 0 && op->getNumResults() == 0)
          p += 3;
        break;
      }
      auto str = printer->str(op->ret(*p++ - '0'));
      if (c == 'R' && str[0] == 'w')
        str[0] = 'x';
      os << str;
      break;
    }
    case 't': {
      char dis = *p++;
      int index = *p++ - '0';
      assert(dis == 'r' || dis == 'x');
      Value *v = dis == 'r' ? op->ret(index) : op->val(index);
      printer->printType(os, v->type);
      break;
    }
    default:
      std::cout << "format str: " << fmt << "\n";
      assert(false && "invalid format string");
    }
  }
}

void Printer::printResults(std::ostream &os, const Op *op, unsigned from) {
  for (size_t i = from; i < op->getNumResults(); i++) {
    os << str(op->ret(i));
    if (i != op->getNumResults() - 1)
      os << ", ";
  }
}

void Printer::printOperands(std::ostream &os, const Op *op, unsigned from) {
  for (size_t i = from; i < op->getNumOperands(); i++) {
    os << str(op->val(i));
    if (i != op->getNumOperands() - 1)
      os << ", ";
  }
}

void Printer::printType(std::ostream &os, const Type *type) {
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
    printType(os, type->pointee());
    os << "*";
    break;
  case Type::fn: {
    os << "(";
    const auto &args = type->argTypes();
    for (auto [i, x] : data::enumerate(args)) {
      printType(os, x);
      if (i + 1 != args.size())
        os << ", ";
    }
    os << ") -> ";
    printType(os, type->retType());
  }
  }
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
format(SubLOp, "$r0 = $x0 - $x1: i64");
format(SubFOp, "$r0 = $x0 - $x1: f32");
format(MulIOp, "$r0 = $x0 * $x1");
format(MulLOp, "$r0 = $x0 * $x1: i64");
format(MulFOp, "$r0 = $x0 * $x1: f32");
format(DivIOp, "$r0 = $x0 / $x1");
format(DivLOp, "$r0 = $x0 / $x1: i64");
format(DivFOp, "$r0 = $x0 / $x1: f32");
format(ModIOp, "$r0 = $x0 % $x1");
format(AndIOp, "$r0 = $x0 & $x1");
format(OrIOp , "$r0 = $x0 | $x1");
format(XorIOp, "$r0 = $x0 ^ $x1");
format(LShiftOp, "$r0 = $x0 << $x1");
format(RShiftOp, "$r0 = $x0 >> $x1");
format(EqOp, "$r0 = $x0 == $x1");
format(NeOp, "$r0 = $x0 != $x1");
format(LtOp, "$r0 = $x0 < $x1");
format(LeOp, "$r0 = $x0 <= $x1");
format(EqFOp, "$r0 = $x0 == $x1");
format(NeFOp, "$r0 = $x0 != $x1");
format(LtFOp, "$r0 = $x0 < $x1");
format(LeFOp, "$r0 = $x0 <= $x1");
format(NotOp, "$r0 = ! $x0");
format(NotFOp, "$r0 = ! $x0");
format(ReturnOp, "return $x0?");
format(ModuleOp, "module");
format(IfOp, "$r>0 = if $x0");
format(WhileOp, "$r>0 = while");
format(ForOp, "$r>1 = for $r0 in range($x0, $x1, $x2)");
format(LoadOp, "$r0 = load $x0");
format(StoreOp, "store [$x0], $x1");
format(CallOp, "$r0 = $x0($x>1)");
format(GetGlobalOp, "$r0 = la $x0");
format(YieldOp, "yield $x>0");
format(ConditionOp, "condition($x0); $x>1");
format(I2FOp, "$r0 = (f32) $x0");
format(F2IOp, "$r0 = (i32) $x0");
format(UndefOp, "\\$r0 = undef $tr0");
format(DoWhileOp, "$r>0 = do-while $x>0");
format(ContinueOp, "continue");
format(BreakOp, "break");
format(SextOp, "$r0 = sext $x0");
format(CondMarkerOp, "<condition begin>");
format(UnreachableOp, "brk #0");
format(MinOp, "$r0 = min($x0, $x1)");
format(MaxOp, "$r0 = max($x0, $x1)");
format(SelectOp, "$r0 = $x0 ? $x1 : $x2");
/* ARM operations */
format(AddWOp, "add $r0, $x0, $x1");
format(AddXOp, "add $R0, $X0, $X1");
format(FaddOp, "fadd $r0, $x0, $x1");
format(AndWOp, "and $r0, $x0, $x1");
format(AndXOp, "and $R0, $X0, $X1");
format(MaddWOp, "madd $r0, $x0, $x1, $x2");
format(MsubWOp, "msub $r0, $x0, $x1, $x2");
format(SubWOp, "sub $r0, $x0, $x1");
format(SubXOp, "sub $R0, $X0, $X1");
format(FsubOp, "fsub $r0, $x0, $x1");
format(MulWOp, "mul $r0, $x0, $x1");
format(MulXOp, "mul $r0, $x0, $x1");
format(FmulOp, "fmul $R0, $X0, $X1");
format(DivWOp, "sdiv $r0, $x0, $x1");
format(DivXOp, "sdiv $R0, $X0, $X1");
format(FdivOp, "fdiv $r0, $x0, $x1");
format(EorWOp, "eor $r0, $x0, $x1");
format(LslWOp, "lsl $r0, $x0, $x1");
format(LsrWOp, "lsr $r0, $x0, $x1");
format(AsrWOp, "asr $r0, $x0, $x1");
format(SxtwOp, "sxtw $r0, $x0");
format(SmullOp, "smull $r0, $x0, $x1");
format(NegOp, "neg $r0, $x0");
format(NegsOp, "negs $r0, $x0");
format(ArmMinOp, "cmp $x0, $x1\ncsel $r0, $x0, $x1, lt");
format(ArmMaxOp, "cmp $x0, $x1\ncsel $r0, $x0, $x1, gt");
format(CmpEqOp, "cmp $x0, $x1\ncset $r0, eq");
format(CmpNeOp, "cmp $x0, $x1\ncset $r0, ne");
format(CmpLtOp, "cmp $x0, $x1\ncset $r0, lt");
format(CmpLeOp, "cmp $x0, $x1\ncset $r0, le");
format(FcmpEqOp, "fcmp $x0, $x1\ncset $r0, eq");
format(FcmpNeOp, "fcmp $x0, $x1\ncset $r0, ne");
format(FcmpLtOp, "fcmp $x0, $x1\ncset $r0, lt");
format(FcmpLeOp, "fcmp $x0, $x1\ncset $r0, le");
format(CsnegMiOp, "csneg $r0, $x0, $x1, mi");
format(FmovOp, "fmov $r0, $x0");
format(FcvtzsOp, "fcvtzs $r0, $x0");
format(ScvtfOp, "scvtf $r0, $x0");
format(RetOp, "ret");

#define iprinter(Ty, name) \
  printer(Ty) { \
    auto x = cast<Ty>(op); \
    os << #name " "; \
    printer->printResults(os, x); \
    if (x->getNumResults() > 0) \
      os << ", "; \
    printer->printOperands(os, x); \
    if (x->getNumOperands() > 0) \
      os << ", "; \
    os << "#" << x->value; \
  }

iprinter(AddWIOp, add)
iprinter(AddXIOp, add)
iprinter(AndWIOp, and)
iprinter(AndXIOp, and)
iprinter(SubWIOp, sub)
iprinter(SubXIOp, sub)
iprinter(EorWIOp, eor)
iprinter(LslWIOp, lsl)
iprinter(AsrWIOp, asr)
iprinter(LsrWIOp, lsr)
iprinter(AsrXIOp, asr)

printer(AddWLslOp) {
  auto x = cast<AddWLslOp>(op);
  os << "add " << printer->str(op->ret()) << ", ";
  os << printer->str(op->val(0)) << ", " << printer->str(op->val(1)) << ", lsl #" << x->value;
}

printer(AddXLslOp) {
  auto x = cast<AddXLslOp>(op);
  os << "add " << printer->str(op->ret()) << ", ";
  os << printer->str(op->val(0)) << ", " << printer->str(op->val(1)) << ", lsl #" << x->value;
}

printer(AddWLsrOp) {
  auto x = cast<AddWLsrOp>(op);
  os << "add " << printer->str(op->ret()) << ", ";
  os << printer->str(op->val(0)) << ", " << printer->str(op->val(1)) << ", lsr #" << x->value;
}

printer(AddSxtOp) {
  auto x = cast<AddSxtOp>(op);
  os << "add " << printer->str(op->ret()) << ", ";
  os << printer->str(op->val(0)) << ", " << printer->str(op->val(1)) << ", sxtw #" << x->value;
}

printer(MovIOp) {
  auto movi = cast<MovIOp>(op);
  os << "mov " << printer->str(op->ret()) << ", #" << movi->value;
}

printer(MovLOp) {
  auto movl = cast<MovLOp>(op);
  os << "mov " << printer->str(op->ret()) << ", #" << movl->value;
}

printer(MovKOp) {
  auto movk = cast<MovKOp>(op);
  os << "movk " << printer->str(op->ret()) << ", #" << movk->value << ", lsl #16";
}

printer(MovZOp) {
  auto movz = cast<MovZOp>(op);
  os << "movz " << printer->str(op->ret()) << ", #" << movz->value;
}

printer(MovNOp) {
  auto movn = cast<MovNOp>(op);
  os << "movn " << printer->str(op->ret()) << ", #" << movn->value;
}

printer(LdrOp) {
  auto ldr = cast<LdrOp>(op);
  os << "ldr " << printer->str(op->ret()) << ", [" << printer->str(op->val()) << ", #" << ldr->value << "]";
}

printer(StrOp) {
  auto str = cast<StrOp>(op);
  os << "str " << printer->str(op->val(1)) << ", [" << printer->str(op->val(0)) << ", #" << str->value << "]";
}

// TODO: Bad in the sense that when `ret == incr` the fallback breaks.
printer(LdrPostIncrOp) {
  auto ldr = cast<LdrPostIncrOp>(op);
  auto addr = printer->str(op->val());
  auto incr = printer->str(op->ret(1));
  auto ret  = printer->str(op->ret(0));
  if (incr == addr) {
    os << "ldr " << ret << ", [" << addr << "], #" << ldr->value;
    return;
  }
  os << "ldr " << ret << ", [" << addr << ", #0]";
  printer->printNewline(os);
  os << "add " << incr << ", " << addr << ", #" << ldr->value << " // fallback: post-indexed";
}

printer(StrPostIncrOp) {
  auto str = cast<StrPostIncrOp>(op);
  auto addr = printer->str(op->val(0));
  auto incr = printer->str(op->ret());
  auto val  = printer->str(op->val(1));
  if (incr == addr) {
    os << "str " << val << ", [" << addr << "], #" << str->value;
    return;
  }
  os << "str " << val << ", [" << addr << ", #0]";
  printer->printNewline(os);
  os << "add " << incr << ", " << addr << ", #" << str->value << " // fallback: post-indexed";
}

printer(LdrPreIncrOp) {
  auto ldr = cast<LdrPreIncrOp>(op);
  auto addr = printer->str(op->val());
  auto incr = printer->str(op->ret(1));
  auto ret  = printer->str(op->ret(0));
  if (incr == addr) {
    os << "ldr " << ret << ", [" << addr << ", #" << ldr->value << "]!";
    return;
  }
  os << "add " << incr << ", " << addr << ", #" << ldr->value << " // fallback: pre-indexed";
  printer->printNewline(os);
  os << "ldr " << ret << ", [" << addr << ", #" << ldr->value << "]";
}

printer(StrPreIncrOp) {
  auto str = cast<StrPreIncrOp>(op);
  auto addr = printer->str(op->val(0));
  auto incr = printer->str(op->ret());
  auto val  = printer->str(op->val(1));
  if (incr == addr) {
    os << "str " << val << ", [" << addr << ", #" << str->value << "]!";
    return;
  }
  os << "add " << incr << ", " << addr << ", #" << str->value << " // fallback: pre-indexed";
  printer->printNewline(os);
  os << "str " << val << ", [" << addr << ", #" << str->value << "]";
}

printer(LdrLslOp) {
  auto ldr = cast<LdrLslOp>(op);
  os << "ldr " << printer->str(op->ret()) << ", ["
     << printer->str(op->val()) << ", " << printer->str(op->val(1));
  
  if (ldr->value)
    os << ", lsl #" << ldr->value;
  os << "]";
}

printer(StrLslOp) {
  auto str = cast<StrLslOp>(op);
  os << "str " << printer->str(op->val(2)) << ", ["
     << printer->str(op->val()) << ", " << printer->str(op->val(1));
  if (str->value)
    os << ", lsl #" << str->value;
  os << "]";
}

printer(LdpOp) {
  auto ldr = cast<LdpOp>(op);
  os << "ldp ";
  printer->printResults(os, ldr);
  os << ", [" << printer->str(op->val()) << ", #" << ldr->value << "]";
}

printer(StpOp) {
  auto str = cast<StpOp>(op);
  os << "stp ";
  printer->printOperands(os, op, 1);
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
  auto call = cast<BlOp>(op);
  os << "bl " << call->name;
  if (op->getNumOperands() > 0) {
    os << "(";
    printer->printOperands(os, op);
    os << ")";
  }
  if (auto r = op->getNumResults(); r > 0) {
    os << " [" << r << " results]";
  }
}

printer(CselLtIOp) {
  auto csel = cast<CselLtIOp>(op);
  os << "cmp " << printer->str(op->val()) << ", #" << csel->value;
  printer->printNewline(os);
  os << "csel " << printer->str(op->ret()) << ", " << printer->str(op->val(1)) << ", " << printer->str(op->val(2)) << ", lt";
}

printer(CnegLtIOp) {
  auto cneg = cast<CnegLtIOp>(op);
  os << "cmp " << printer->str(op->val(0)) << ", #" << cneg->value;
  printer->printNewline(os);
  os << "cneg " << printer->str(op->ret()) << ", " << printer->str(op->val(1)) << ", lt";
}

#define arm_branch_printer(Ty) \
  printer(Ty) { \
    auto br = cast<Ty>(op); \
    std::string x = #Ty; \
    x[0] = tolower(x[0]); \
    x = x.substr(0, x.size() - 2); /* Remove the final 'Op'. */ \
    if (x[0] == 'c') { /* `cbz` and `cbnz` take one register. */ \
      os << x << ' '; \
      printer->printOperands(os, op); \
      os << ", " << printer->str(br->target); \
      if (br->other) \
        os << ", " << printer->str(br->other); \
      return; \
    } \
    assert(x[0] == 'b'); /* b.eq set of instructions need a cmp before them. */ \
    x.insert(x.begin() + 1, '.'); /* Change `beq` to `b.eq`. */ \
    if (x.back() == 'F') /* BeqFOp will generate `b.eqF`. We need to remove the `f`. */ \
      os << "fcmp ", x.pop_back(); \
    else \
      os << "cmp "; \
    printer->printOperands(os, op); \
    printer->printNewline(os); \
    os << x << ' ' << printer->str(br->target); \
    if (br->other) \
      os << ", " << printer->str(br->other); \
  }

arm_branch_op_list(arm_branch_printer)

printer(ExternCallOp) {
  auto extc = cast<ExternCallOp>(op);
  printer->printResults(os, op);
  if (extc->getNumResults() > 0)
    os << " = ";
  os << extc->name << "(";
  printer->printOperands(os, op);
  os << ')';
}

printer(GlobalOp) {
  auto glob = cast<GlobalOp>(op);
  os << printer->str(glob->ret()) << " = " << glob->name << ": ";
  printer->printType(os, glob->ret()->type);
}

printer(AllocaOp) {
  auto ret = op->ret();
  os << printer->str(ret) << " = alloca ";
  printer->printType(os, ret->type->pointee());
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
  printer->printResults(os, fn, 1);
  os << ")";
}

printer(IntOp) {
  os << printer->str(op->ret(0)) << " = " << cast<IntOp>(op)->value;
}

printer(Int64Op) {
  os << printer->str(op->ret(0)) << " = " << cast<Int64Op>(op)->value << "ll";
}

printer(FloatOp) {
  os << printer->str(op->ret(0)) << " = " << cast<FloatOp>(op)->value;
}

printer(JumpOp) {
  auto jmp = cast<JumpOp>(op);
  os << "=> " << printer->str(jmp->target);
  if (jmp->getNumOperands() > 0) {
    os << "(";
    printer->printOperands(os, jmp);
    os << ")";
  }
}

printer(BranchOp) {
  auto br = cast<BranchOp>(op);
  os << "=> " << printer->str(op->val(0)) << " ? " << printer->str(br->target) << " : " << printer->str(br->other);
  if (br->getNumOperands() > 1) {
    os << " with ";
    printer->printOperands(os, br, 1);
  }
}

printer(PhiOp) {
  auto phi = cast<PhiOp>(op);
  os << "phi " << printer->str(phi->ret()) << ' ';
  for (auto [value, block] : *phi)
    os << "[ " << printer->str(value) << ", " << printer->str(block) << " ] ";
}

printer(WriteRegOp) {
  auto wr = cast<WriteRegOp>(op);
  auto val = wr->val();
  std::string name = regname(wr->reg);
  std::string opname = "mov ";
  if (val->type == i32) {
    assert(name[0] == 'x' || name[0] == '%');
    name[0] = 'w';
  }
  if (val->type == f32) {
    assert(name[0] == 'v' || name[0] == '%');
    name[0] = 's';
    opname = "fmov ";
  }

  auto valreg = printer->str(val);
  if (valreg == name && !printer->showHidden)
    return;
  os << opname << name << ", " << valreg;
}

printer(ReadRegOp) {
  auto wr = cast<ReadRegOp>(op);
  auto ret = wr->ret();
  std::string name = regname(wr->reg);
  std::string opname = "mov ";
  if (ret->type == i32) {
    assert(name[0] == 'x' || name[0] == '%');
    name[0] = 'w';
  }
  if (ret->type == f32) {
    assert(name[0] == 'v' || name[0] == '%');
    name[0] = 's';
    opname = "fmov ";
  }

  auto retreg = printer->str(ret);
  if (retreg == name && !printer->showHidden)
    return;
  os << opname << retreg << ", " << name;

  if (printer->showHidden && op->getNumOperands() > 0) {
    os << ": colliding with ";
    for (auto x : op->getOperands())
      os << printer->str(x) << " ";
  }
}

printer(CastOp) {
  auto retreg = printer->str(op->ret()), fromreg = printer->str(op->val());
  auto r = retreg; r[0] = 'x';
  auto f = fromreg; f[0] = 'x';
  if (!printer->showHidden) {
    if (r == f)
      return;
  }
  os << "mov " << r << ", " << f;
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

attr_printer(StackOffsetAttr) {
  os << "<offset = " << cast<StackOffsetAttr>(attr)->size << ">";
}

attr_printer(ArgDimAttr) {
  auto arg = cast<ArgDimAttr>(attr);
  os << "<dims: ";
  for (auto [k, v] : arg->dims)
    os << printer->str(k) << ": [" << v << "], ";
  os << "end>";
}

attr_printer(ConstIArrAttr) {
  auto iarr = cast<ConstIArrAttr>(attr);
  os << "<arr = ";
  if (iarr->value.size() > iarr->zeroSuffix)
    os << iarr->value[0];
  for (unsigned i = 1; i + iarr->zeroSuffix < iarr->value.size(); i++)
    os << ", " << iarr->value[i];
  if (iarr->zeroSuffix > 0)
    os << ", " << iarr->zeroSuffix << " x 0";
  os << ">"; 
}

attr_printer(ConstFArrAttr) {
  auto farr = cast<ConstFArrAttr>(attr);
  os << "<arr = ";
  if (farr->value.size() > farr->zeroSuffix)
    os << farr->value[0];
  for (unsigned i = 1; i < farr->value.size() - farr->zeroSuffix; i++)
    os << ", " << farr->value[i];
  if (farr->zeroSuffix > 0)
    os << ", " << farr->zeroSuffix << " x 0";
  os << ">"; 
}

attr_printer(NonIdempotentAttr) {
  (void) attr;
  os << "<non-idempotent>";
}

attr_printer(UnerasableAttr) {
  (void) attr;
  os << "<no erase>";
}

attr_printer(RecursiveAttr) {
  (void) attr;
  os << "<rec>";
}

attr_printer(IncomingStackArgAttr) {
  (void) attr;
  os << "<stackarg>";
}

attr_printer(UnreachableAttr) {
  os << "<unreachable: " << cast<UnreachableAttr>(attr)->region << ">";
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
  std::stringstream ss;
  std::string name = (it == idents.end()) ? "%" + std::to_string(id(value)) : it->second;
  if (options.printType) {
    ss << "(" << name << ": ";
    printType(ss, value->type);
    ss << ")";
  } else
    ss << name;
  
  if (options.ranges && showRange && printing) {
    auto it = opt::rangeResult.find(printing);
    if (it != opt::rangeResult.end()) {
      data::ConstEnv env = it->second;
      if (auto v = env.data->find(value); v != env.data->end()) {
        data::Interval i = v->second;
        ss << i;
      }
    }
  }

  return ss.str();
}

std::string Printer::str(const Block *block) {
  return bbPrefix + std::to_string(id(block));
}

void Printer::indent(std::ostream &os) {
  for (int i = 0; i < depth; i++)
    os << "  ";
}

void Printer::printNewline(std::ostream &os) {
  os << "\n";
  indent(os);
}

void Printer::printImpl(const Block *bb, bool tag) {
  auto numArgs = bb->getNumArgs();
  if (tag || numArgs != 0 || bbPrefix != "bb") {
    depth--;
    indent(os);
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
  indent(os);
  os << "}";
}

void Printer::print(const Op *op) {
  auto printfn = dispatch()[op->id];
  assert(printfn);
  printing = op;
  
  std::stringstream ss("");
  printfn(ss, op, this);
  auto str = ss.str();

  if (!str.empty())
    indent(os);
  os << str;
  for (const auto &[_, attr] : op->getAttrs())
    print(attr);
  for (auto r : op->getRegions())
    print(r);
  if (!str.empty())
    os << '\n';
}

void Printer::print(const Attr *attr) {
  if (!showAttr)
    return;
  
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

std::ostream &operator<<(std::ostream &os, const Op *op) {
  if (!op)
    return os << "<null op>\n";
  
  printer.print(op);
  printer.dump(os);
  return os;
}

std::ostream &operator<<(std::ostream &os, const Value *v) {
  if (!v)
    return os << "<null value>";

  return os << printer.str(v);
}

std::ostream &operator<<(std::ostream &os, const Block *bb) {
  if (!bb)
    return os << "<null block>";

  printer.print(bb);
  printer.dump(os);
  return os;
}

std::ostream &operator<<(std::ostream &os, const Region *region) {
  if (!region)
    return os << "<null region>";

  printer.print(region);
  printer.dump(os);
  return os;
}

std::ostream &operator<<(std::ostream &os, const Type *type) {
  if (!type)
    return os << "<null type>";

  printer.printType(os, type);
  return os;
}

}

