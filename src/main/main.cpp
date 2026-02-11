#include "Options.h"

#include "../parse/Parser.h"
#include "../parse/Sema.h"

#include "../ir/CodeGen.h"

#include "../opt/high/Passes.h"
#include "../opt/mid/Passes.h"
#include "../opt/low/Passes.h"

#include <fstream>
#include <sstream>
#include <iostream>

Options options;

void populate(opt::PassManager &pm) {
  // High-IR passes.
  add_pass(EnsureTerminator);
  add_pass(Mem2Reg);
  add_pass(HighDCE);

  // Mid-IR passes.
  add_pass(Flatten);

  // Low-IR passes.
  add_pass(Lower);
  add_pass(InstCombine);
  add_pass(LowDCE);
  add_pass(SimplifyCFG);
  add_pass(LowerPostSchedule);
  add_pass(RegAlloc);
  add_pass(DestroyPhi);
  add_pass(SimplifyCFG);
  add_pass(LateLegalize);
  add_pass(Print);
}

int main(int argc, char **argv) {
  options = parseArgs(argc, argv);

  // Read input file.
  std::ifstream ifs(options.inputFile);
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
  delete node;

  opt::PassManager pm(module);
  populate(pm);
  pm.run();
  return 0;
}
