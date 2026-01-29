#ifndef CODEGEN_H
#define CODEGEN_H

#include "Builder.h"
#include "../parse/ASTNode.h"

namespace ir {

class CodeGen {
  using SymbolTable = std::map<std::string, Value*>;
  SymbolTable table;
  Builder builder;
  ModuleOp *module;

public:
  class Guard {
    CodeGen *cg;
    SymbolTable table;
  public:
    Guard(CodeGen *cg): cg(cg), table(cg->table) {}
    ~Guard() { cg->table = table; }
  };

  CodeGen();
  void emitStmt(ast::ASTNode *node);
  Value *emitExpr(ast::ASTNode *node);
  ModuleOp *emitModule(ast::ASTNode *node);
};

}

#endif
