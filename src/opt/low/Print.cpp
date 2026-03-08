#include "Common.h"
#include "../../main/Options.h"
#include <fstream>

namespace opt {

declare_pass(Print) {
  for (auto [v, r] : assignment) {
    std::string name = regname(r);
    if (v->type == i32)
      name[0] = 'w';
    if (v->type == f32)
      name[0] = 's';

    printer.addIdent(v, name);
  }
  printer.showHidden = false;

  std::ofstream f;
  std::ostream &os = options.outputFile == "-" || options.outputFile == ""
    ? (std::ostream &) std::cout
    : (f.open(options.outputFile), f);
  
  printer.setIndent(1);
  os << ".text\n";
  os << ".global main\n";
  os << ".type main, %function\n";
  for (auto x : collectFunctions()) {
    os << x->name << ":\n";
    for (auto bb : *x->getRegion())
      os << bb;
    os << "\n";
  }

  for_all(GlobalOp) {
    if (auto iarr = op->get<ConstIArrAttr>()) {
      os << (iarr->allZeroes() ? ".bss\n" : ".data\n");
      os << ".align 3\n";
      os << op->name << ":\n";
      if (iarr->allZeroes())
        os << "  .zero " << asmSize(op) << "\n";
      else {
        os << "  .int " << iarr->value[0];
        for (unsigned i = 1; i < iarr->value.size(); i++)
          os << ", " << iarr->value[i];
        os << "\n";
      }
    } else {
      os << ".bss\n.align 3\n";
      os << op->name << ":\n";
      os << "  .zero " << asmSize(op) << "\n";
    }
  }
}

}