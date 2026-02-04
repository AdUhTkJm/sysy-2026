#ifndef CODEGEN_H
#define CODEGEN_H

#include "Builder.h"
#include "../parse/ASTNode.h"

namespace ir {

class CodeGen {
  using SymbolTable = std::map<std::string, Value*>;
  SymbolTable table, globals;
  Builder builder;
  ModuleOp *module;

  static constexpr std::string constructor = "__init";
  static const std::map<std::string, Type *> external;

  Value *getGlobal(const std::string &name);
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
