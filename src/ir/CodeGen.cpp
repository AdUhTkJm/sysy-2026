#include "CodeGen.h"
#include "Ops.h"
#include "Attrs.h"

using namespace ast;
using namespace data;

namespace ir {

const std::map<std::string, const Type*> CodeGen::external = {
  { "putint", unit },
  { "putch", unit },
  { "putfloat", unit },
  { "putfarray", unit },
  { "putarray", unit },
  { "getint", i32 },
  { "getch", i32 },
  { "getarray", i32 },
  { "getfarray", i32 },
  { "getfloat", f32 },
  { "_sysy_starttime", unit },
  { "_sysy_stoptime", unit },
};

static const Type *convert(ast::Type *type) {
  if (isa<IntType>(type))
    return i32;
  if (isa<FloatType>(type))
    return f32;
  if (isa<VoidType>(type))
    return unit;

  if (auto p = dyn_cast<PointerType>(type))
    return Type::pointer(convert(p->pointee));
  
  // The dimension of array is not important. We will generate this
  // on array access.
  if (auto arr = dyn_cast<ArrayType>(type))
    return Type::pointer(convert(arr->base));

  if (auto fnTy = dyn_cast<FunctionType>(type)) {
    Types args;
    args.push_back(convert(fnTy->ret));
    args.reserve(fnTy->params.size());
    for (auto x : fnTy->params)
      args.push_back(convert(x));
    return Type::function(args);
  }

  std::cout << type->toString() << "\n";
  assert(false && "unknown type");
}

CodeGen::CodeGen(): module(builder.createModule()) {
  auto bb = module->createFirstBlock();
  builder.setToStart(bb);
  auto init = builder.create<FuncOp>(Type::function(unit, {}));
  
  // Create a dummy init function that has only a return instruction.
  table[init->name = constructor] = init->ret();
  auto initbb = init->createFirstBlock();
  builder.setToStart(initbb);
  builder.create<ReturnOp>();

  // Point the builder to the end of the module,
  // where new functions will be generated.
  builder.setToEnd(bb);
}

Value *CodeGen::getGlobal(const std::string &str) {
  assert(globals.count(str));
  auto ptr = globals[str];
  auto get = builder.create<GetGlobalOp>(Type::pointer(ptr->type))->with(ptr);
  return get->ret();
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
    Value *addr;
    if (!table.count(ref->name))
      addr = getGlobal(ref->name);
    else
      addr = table[ref->name];
    return builder.create<LoadOp>(addr->type->pointee())->with(addr)->ret();
  }

  if (auto bin = dyn_cast<BinaryNode>(node)) {
    if (bin->kind == BinaryNode::Or) {
      Value *l = emitExpr(bin->l);
      auto br = builder.create<IfOp>(i32)->with(l);
      auto ifso = br->appendRegion()->getFirstBlock();
      Builder::Guard guard(builder);
      builder.setToStart(ifso);
      auto one = builder.createInt(1);
      builder.create<YieldOp>()->with(one->ret());
      
      auto ifnot = br->appendRegion()->getFirstBlock();
      builder.setToStart(ifnot);
      Value *r = emitExpr(bin->r);
      builder.create<YieldOp>()->with(r);
      return br->ret();
    }
    if (bin->kind == BinaryNode::And) {
      Value *l = emitExpr(bin->l);
      auto br = builder.create<IfOp>(i32)->with(l);
      auto ifso = br->appendRegion()->getFirstBlock();
      Builder::Guard guard(builder);
      builder.setToStart(ifso);
      Value *r = emitExpr(bin->r);
      builder.create<YieldOp>()->with(r);
      
      auto ifnot = br->appendRegion()->getFirstBlock();
      builder.setToStart(ifnot);
      auto zero = builder.createInt(0);
      builder.create<YieldOp>()->with(zero->ret());
      return br->ret();
    }

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
    case BinaryNode::Eq:
      return builder.create<EqOp>(i32)->with(l, r)->ret();
    case BinaryNode::Ne:
      return builder.create<NeOp>(i32)->with(l, r)->ret();
    case BinaryNode::Lt:
      return builder.create<LtOp>(i32)->with(l, r)->ret();
    case BinaryNode::Le:
      return builder.create<LeOp>(i32)->with(l, r)->ret();
    default:
      std::cout << "unknown binary type: " << bin->kind << "\n";
      assert(false);
    }
  }

  if (auto un = dyn_cast<UnaryNode>(node)) {
    Value *v = emitExpr(un->node);
    switch (un->kind) {
    case UnaryNode::Minus: {
      auto zero = builder.createInt(0);
      return builder.create<SubIOp>(i32)->with(zero->ret(), v)->ret();
    }
    case UnaryNode::Not:
      return builder.create<NotOp>(i32)->with(v)->ret();
    case UnaryNode::Float2Int:
      return builder.create<F2IOp>(f32)->with(v)->ret();
    case UnaryNode::Int2Float:
      return builder.create<I2FOp>(f32)->with(v)->ret();
    default:
      std::cout << "unknown unary type: " << un->kind << "\n";
      assert(false);
    }
  }

  if (auto access = dyn_cast<ArrayAccessNode>(node)) {
    Value *array;
    if (auto it = table.find(access->array); it == table.end())
      array = getGlobal(access->array);
    else
      array = it->second;
    Values vals { array };
    for (auto x : access->indices)
      vals.push_back(emitExpr(x));
    return builder.create<ArrayLoadOp>(array->type->pointee())->with(vals)->ret();
  }

  if (auto call = dyn_cast<CallNode>(node)) {
    if (auto it = external.find(call->func); it != external.end()) {
      Values vals;
      for (auto x : call->args)
        vals.push_back(emitExpr(x));
      auto extc = builder.create<ExternCallOp>(it->second)->with(vals);
      extc->name = call->func;
      return extc->ret();
    }

    assert(globals.count(call->func));
    auto func = globals[call->func];
    Values vals { func };
    for (auto x : call->args)
      vals.push_back(emitExpr(x));
    return builder.create<CallOp>(func->type->retType())->with(vals)->ret(); 
  }

  std::cout << "node unknown: " << node->getID() << "\n";
  assert(false);
}

void CodeGen::emitStmt(ASTNode *node) {
  if (auto fn = dyn_cast<FnDeclNode>(node)) {
    auto fnType = convert(fn->type);
    Types types { fnType };
    concat(types, fnType->argTypes());
    auto func = builder.create<FuncOp>(types);
    func->name = std::move(fn->name);
    globals[func->name] = func->getHandle();
  
    // Now dive into the function body.
    Guard guard(this);
    Builder::Guard _(builder);
    
    auto bb = func->createFirstBlock();
    builder.setToStart(bb);
    for (const auto &[i, name] : data::enumerate(fn->args)) {
      auto alloca = builder.create<AllocaOp>(Type::pointer(types[i + 1]));
      table[name] = alloca->ret();
      builder.create<StoreOp>()->with(alloca->ret(), func->getArg(i));
    }
    emitStmt(fn->body);
    if (bb->getNumOps() == 0 || !isa<ReturnOp>(bb->getLastOp())) {
      builder.setToEnd(bb);
      builder.create<ReturnOp>();
    }
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

  if (auto access = dyn_cast<ArrayAssignNode>(node)) {
    Value *array;
    if (auto it = table.find(access->array); it == table.end())
      assert(globals.count(access->array)), array = globals[access->array];
    else
      array = it->second;
    Values vals { array };
    for (auto x : access->indices)
      vals.push_back(emitExpr(x));
    vals.push_back(emitExpr(access->value));
    builder.create<ArrayStoreOp>()->with(vals);
    return;
  }

  if (auto ret = dyn_cast<ReturnNode>(node)) {
    auto val = emitExpr(ret->node);
    builder.create<ReturnOp>()->with(val);
    return;
  }

  if (auto decl = dyn_cast<VarDeclNode>(node); decl && !decl->global) {
    auto arrTy = dyn_cast<ArrayType>(decl->type);
    auto allocaTy = Type::pointer(convert(arrTy ? arrTy->base : decl->type));
    auto alloca = builder.create<AllocaOp>(allocaTy);
    table[decl->name] = alloca->ret();

    if (!arrTy) {
      if (decl->init) {
        auto init = emitExpr(decl->init);
        builder.create<StoreOp>()->with(alloca->ret(), init);
      }
      return;
    }

    // Now deal with arrays.
    // This attribute is mainly for lowering.
    alloca->set<DimAttr>(arrTy->dims);
    if (!decl->init)
      return;

    // Look at initializers.
    if (auto arr = dyn_cast<LocalArrayNode>(decl->init)) {
      auto arrTy = cast<ArrayType>(node->type);
      for (auto i = 0, e = arrTy->getSize(); i < e; i++) {
        if (!arr->va[i])
          continue;
        auto value = emitExpr(arr->va[i]);
        // Find indices of this `i`.
        Values indices { alloca->ret() };
        indices.reserve(arrTy->dims.size() + 2);
        auto j = i;
        for (auto dim : data::reverse(arrTy->dims)) {
          indices.push_back(builder.createInt(j % dim)->ret());
          j /= dim;
        }
        indices.push_back(value);
        builder.create<ArrayStoreOp>()->with(indices);
      }
      return;
    }

    if (auto arr = dyn_cast<ConstArrayNode>(decl->init)) {
      auto arrTy = cast<ArrayType>(node->type);
      for (auto i = 0, e = arrTy->getSize(); i < e; i++) {
        Values indices { alloca->ret() };
        indices.reserve(arrTy->dims.size() + 2);
        auto j = i;
        for (auto dim : data::reverse(arrTy->dims)) {
          indices.push_back(builder.createInt(j % dim)->ret());
          j /= dim;
        }
        Op *op = isa<FloatType>(arrTy->base)
          ? (Op*) builder.createFloat(arr->vf[i])
          : builder.createInt(arr->vi[i]);
        indices.push_back(op->ret());
        builder.create<ArrayStoreOp>()->with(indices);
      }
      return;
    }
    assert(false && "unexpected array init (local)");
  }

  // This is a global node.
  if (auto decl = dyn_cast<VarDeclNode>(node)) {
    auto arrTy = dyn_cast<ArrayType>(decl->type);
    if (!arrTy) {
      auto ty = convert(decl->type);
      auto global = builder.create<GlobalOp>(ty);
      global->name = decl->name;
      globals[decl->name] = global->ret();
      if (!decl->init)
        return;

      Builder::Guard guard(builder);
      builder.setToStart(table[constructor]->def->getRegion()->getFirstBlock());
      auto value = emitExpr(decl->init);
      auto la = builder.create<GetGlobalOp>(Type::pointer(ty))->with(global->ret());
      builder.create<StoreOp>()->with(la->ret(), value);
      return;
    }
    auto baseTy = convert(arrTy->base);
    auto arr = cast<ConstArrayNode>(decl->init);
    auto global = builder.create<GlobalArrayOp>(baseTy);
    globals[decl->name] = global->ret();

    if (baseTy != f32) {
      std::vector<int> r;
      r.reserve(arrTy->getSize());
      for (int i = 0; i < arrTy->getSize(); i++)
        r.push_back(arr->vi[i]);
      global->set<ConstIArrAttr>(r);
    } else {
      std::vector<float> r;
      r.reserve(arrTy->getSize());
      for (int i = 0; i < arrTy->getSize(); i++)
        r.push_back(arr->vf[i]);
      global->set<ConstFArrAttr>(r);
    }
    return;
  }

  if (auto assign = dyn_cast<AssignNode>(node)) {
    auto name = cast<VarRefNode>(assign->l)->name;
    Value *addr;
    if (auto it = table.find(name); it != table.end())
      addr = it->second;
    else
      addr = getGlobal(name);
    auto value = emitExpr(assign->r);
    builder.create<StoreOp>()->with(addr, value);
    return;
  }

  if (auto br = dyn_cast<IfNode>(node)) {
    auto cond = emitExpr(br->cond);
    auto op = builder.create<IfOp>()->with(cond);
    auto ifso = op->appendRegion();
    
    // We guarantee that each IfOp have 2 regions, even though
    // the else branch is not always existent in AST.
    Builder::Guard guard(builder);
    builder.setToStart(ifso);
    emitStmt(br->ifso);
    builder.create<YieldOp>();
    
    auto ifnot = op->appendRegion();
    builder.setToStart(ifnot);
    if (br->ifnot) 
      emitStmt(br->ifnot);
    builder.create<YieldOp>();
    return;
  }

  if (auto loop = dyn_cast<WhileNode>(node)) {
    // We always generate a rotated loop instead.
    Builder::Guard guard(builder);

    // Generate the if-guard of loop condition.
    auto cond = emitExpr(loop->cond);
    auto br = builder.create<IfOp>()->with(cond);

    // The ifnot part is straightforward: nothing happens.
    auto ifso = br->appendRegion();
    auto ifnot = br->appendRegion();
    builder.setToStart(ifnot);
    builder.create<YieldOp>();

    // The ifso part contains the do-while loop.
    builder.setToStart(ifso);
    auto op = builder.create<DoWhileOp>();
    builder.create<YieldOp>();
    
    auto region = op->appendRegion();
    builder.setToStart(region);
    emitStmt(loop->body);
    cond = emitExpr(loop->cond);
    builder.create<ConditionOp>()->with(cond);
    return;
  }

  emitExpr(node);
}

}
