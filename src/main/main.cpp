#include "Options.h"

#include "../parse/Parser.h"
#include "../parse/Sema.h"

#include <fstream>
#include <sstream>
#include <iostream>

sys::Options opts;

int main(int argc, char **argv) {
  opts = sys::parseArgs(argc, argv);

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

  sys::TypeContext ctx;

  sys::Parser parser(ss.str(), ctx);
  sys::ASTNode *node = parser.parse();
  sys::Sema sema(node, ctx);

  return 0;
}
