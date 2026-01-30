#include "Options.h"

#include "../parse/Parser.h"
#include "../parse/Sema.h"

#include "../ir/CodeGen.h"
#include "../ir/Printer.h"

#include "../opt/high/Passes.h"

#include <fstream>
#include <sstream>
#include <iostream>

Options opts;

void populate(opt::PassManager &pm) {
  add_pass(EnsureTerminator);
  add_pass(Mem2Reg);
}

int main(int argc, char **argv) {
  opts = parseArgs(argc, argv);

  // Read input file.
  std::ifstream ifs(opts.inputFile);
  if (!ifs) {
    std::cerr << "cannot open file\n";
    return 1;
  }

  std::stringstream ss;
  // Add a newline at the end.
  // Single-line comments cannot terminate with EOF.
  ss << ifs.rdbuf() << "\n";

  ast::TypeContext ctx;

  ast::Parser parser(ss.str(), ctx);
  ast::ASTNode *node = parser.parse();
  ast::Sema sema(node, ctx);

  ir::CodeGen cg;
  auto module = cg.emitModule(node);

  opt::PassManager pm(module, opts);
  populate(pm);
  pm.run();
  std::cout << module << "\n";
  return 0;
}
