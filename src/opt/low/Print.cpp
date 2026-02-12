#include "Common.h"
#include "../../main/Options.h"
#include <fstream>

namespace opt {

declare_pass(Print) {
  for (auto [v, r] : assignment) {
    std::string name = regname(r);
    if (v->type == i32) {
      assert(name[0] == 'x');
      name[0] = 'w';
    }

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

  os << ".bss\n";
  for_all(GlobalOp) {
    os << ".align 3\n";
    os << op->name << ":\n";
    os << "  .zero " << asmSize(op) << "\n";
  }
}

}