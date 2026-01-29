#include "CodeGen.h"
#include "Ops.h"

using namespace ast;
using namespace data;

namespace ir {

static Type *convert(ast::Type *type) {
  if (isa<IntType>(type))
    return i32;
  if (isa<FloatType>(type))
    return f32;
  if (auto p = dyn_cast<PointerType>(type))
    return new Type(Type::ptr, { convert(p->pointee) });
  if (auto fnTy = dyn_cast<FunctionType>(type)) {
    std::vector<Type*> args;
    args.push_back(convert(fnTy->ret));
    args.reserve(fnTy->params.size());
    for (auto x : fnTy->params)
      args.push_back(convert(x));
    return new Type(Type::fn, args);
  }
  assert(false && "unknown type");
}

CodeGen::CodeGen(): module(builder.createModule()) {
  builder.setToStart(module->createFirstBlock());
}

ModuleOp *CodeGen::emitModule(ASTNode *node) {
  auto block = cast<BlockNode>(node);
  for (auto n : block->nodes)
    emitStmt(n);
  return module;
}

Value *CodeGen::emitExpr(ASTNode *node) {
  if (auto i = dyn_cast<IntNode>(node))
    return builder.createInt(i->value)->ret();
  if (auto f = dyn_cast<FloatNode>(node))
    return builder.createFloat(f->value)->ret();

  if (auto ref = dyn_cast<VarRefNode>(node)) {
    assert(table.count(ref->name));
    auto alloca = table[ref->name];
    return builder.create<LoadOp>(alloca->type->pointee())->with(alloca)->ret();
  }

  if (auto bin = dyn_cast<BinaryNode>(node)) {
    Value *l = emitExpr(bin->l), *r = emitExpr(bin->r);
    // TODO: what about floating point?
    switch (bin->kind) {
    case BinaryNode::Add:
      return builder.create<AddIOp>(i32)->with(l, r)->ret();
    case BinaryNode::Sub:
      return builder.create<SubIOp>(i32)->with(l, r)->ret();
    case BinaryNode::Mul:
      return builder.create<MulIOp>(i32)->with(l, r)->ret();
    case BinaryNode::Div:
      return builder.create<DivIOp>(i32)->with(l, r)->ret();
    case BinaryNode::Mod:
      return builder.create<ModIOp>(i32)->with(l, r)->ret();
    default:
      std::cout << "binary type unknown: " << bin->kind << "\n";
      assert(false);
    }
  }

  std::cout << "node unknown: " << node->getID() << "\n";
  assert(false);
}

void CodeGen::emitStmt(ASTNode *node) {
  if (auto fn = dyn_cast<FnDeclNode>(node)) {
    auto fnType = convert(fn->type);
    std::vector<Type*> types { fnType };
    concat(types, fnType->argTypes());
    auto func = builder.create<FuncOp>(types);
    func->name = std::move(fn->name);
    table[func->name] = func->getHandle();
  
    // Now dive into the function body.
    Guard guard(this);
    Builder::Guard _(builder);
    for (const auto &[i, name] : data::enumerate(fn->args))
      table[name] = func->getArg(i);
    
    builder.setToStart(func->createFirstBlock());
    emitStmt(fn->body);
    return;
  }

  if (auto block = dyn_cast<BlockNode>(node)) {
    Guard guard(this);
    for (auto x : block->nodes)
      emitStmt(x);
    return;
  }

  if (auto block = dyn_cast<TransparentBlockNode>(node)) {
    for (auto x : block->nodes)
      emitStmt(x);
    return;
  }

  if (auto ret = dyn_cast<ReturnNode>(node)) {
    auto val = emitExpr(ret->node);
    builder.create<ReturnOp>()->with(val);
    return;
  }

  if (auto decl = dyn_cast<VarDeclNode>(node)) {
    auto varTy = convert(decl->type);
    auto allocaTy = new Type(Type::ptr, { varTy });
    table[decl->name] = builder.create<AllocaOp>(allocaTy)->ret();
    return;
  }

  emitExpr(node);
}

}
