#include "Matcher.h"
#include "Printer.h" // IWYU pragma: keep
#include <cstring>

namespace {

enum Kind {
  LPar, RPar, Ident, Colon, End
};

struct Token {
  Kind kind;
  std::string_view text;
};

class Lexer {
  std::string_view str;
  unsigned loc = 0;
public:
  Lexer(std::string_view str): str(str) {}

  Token next();
};

Token Lexer::next() {
  while (loc < str.size() && isspace(str[loc]))
    loc++;
  
  if (loc == str.size())
    return { End, "" };

  auto c = str[loc++];
  if (c == '(')
    return { LPar, "" };
  if (c == ')')
    return { RPar, "" };
  if (c == ':')
    return { Colon, "" };

  unsigned v = --loc;
  for (; v < str.size(); v++) {
    if (!(isalnum(str[v]) || str[v] == '_' || str[v] == '\'' || str[v] == '!'))
      break;
  }
  Token t { Ident, str.substr(loc, v - loc) };
  loc = v;
  return t;
}

using namespace ir::match;

class Parser {
  Lexer &lex;
  Token cur;

  void expect(Kind k);
  std::string_view expectIdent();
  void advance() { cur = lex.next(); }
public:
  const Pattern *parse();
  Parser(Lexer &lex): lex(lex), cur(lex.next()) {}
};

void Parser::expect(Kind k) {
  assert(cur.kind == k);
  advance();
}

std::string_view Parser::expectIdent() {
  assert(cur.kind == Ident);
  auto text = cur.text;
  advance();
  return text;
}

const Pattern *Parser::parse() {
  if (cur.kind == Ident) {
    std::string_view name = cur.text;
    advance();

    // A number literal.
    if (name[0] <= '9' && name[0] >= '0') {
      auto pat = new Pattern(ActionKind::literal);
      int value = 0;
      for (int i = name.size() - 1; i >= 0; i--)
        value = value * 10 + name[i] - '0';
      pat->children[0] = (Pattern*) (unsigned long) value;
      return pat;
    } 

    auto pat = new Pattern(name[0] == '\'' ? Pattern::Imm : Pattern::Var, name);

    if (cur.kind == Colon) {
      advance();
      auto tyname = expectIdent();
      assert(tyname.size() < 4);
      strncpy(pat->tyname, tyname.begin(), tyname.size());
    }
    return pat;
  }

  expect(LPar);
  Pattern *pat;
  auto opname = expectIdent();
  if (opname[0] == '!')
    pat = new Pattern(actionNames.at(std::string(opname)));
  else
    pat = new Pattern(ir::opkindNames.at(std::string(opname)));

  if (cur.kind == Colon) {
    advance();
    auto tyname = expectIdent();
    assert(tyname.size() < 4);
    strncpy(pat->tyname, tyname.begin(), tyname.size());
  }

  for (int i = 0; i < 3 && cur.kind != RPar; i++)
    pat->children[i] = parse();

  expect(RPar);
  return pat;
}

}

namespace ir::match {

Arena Pattern::arena;

#define actname(Ty, ...) { "!" #Ty, ActionKind::Ty },
const std::map<std::string, ActionKind> actionNames {
  action_list(actname)
};

void Env::refillTypes() {
  types["i32"] = i32;
  types["i64"] = i64;
  types["f32"] = f32;
  types["vi4"] = vi4;
  types["vf4"] = vf4;
}

void Env::clear() {
  vals.clear();
  imms.clear();
  types.clear();
  refillTypes();
}

int Pattern::size() const {
  if (kind == Var || kind == Imm)
    return 0;

  int size;
  for (size = 0; size < 3; size++) {
    if (!children[size])
      break;
  }
  return size;
}

const Pattern *Pattern::make(std::string_view str) {
  Lexer lex(str);
  Parser parser(lex);
  return parser.parse();
}

Pattern::Pattern(decltype(kind) k, std::string_view n): kind(k) {
  assert(n.size() < sizeof(name));
  strncpy(name, n.begin(), n.size());
}

Pattern::Pattern(OpKind kind): kind(Op), op(kind) {
}

Pattern::Pattern(ActionKind kind): kind(Action), act(kind) {
}

bool matchVar(Value *v, const Pattern *pattern, Env &env) {
  if (!v)
    return false;

  // We must record the operation's type regardless of pattern kind.
  if (strlen(pattern->tyname) != 0) {
    if (auto it = env.types.find(pattern->tyname); it != env.types.end()) {
      if (it->second != v->type)
        return false;
    }

    env.types[pattern->tyname] = v->type;
  }

  // Then only deal with variables.
  if (pattern->kind != Pattern::Var)
    return false;
  
  if (auto it = env.vals.find(pattern->name); it != env.vals.end()) {
    if (it->second != v)
      return false;
  } else
    env.vals[pattern->name] = v;
  
  return true;
}

Value *buildVar(Builder &, const Pattern *pattern, const Env &env) {
  if (pattern->kind == Pattern::Var)
    return env.vals.at(pattern->name);

  return nullptr;
}

static bool match(Value *v, const Pattern *pattern, Env &env) {
  auto it = adaptors.find(v->def->id);
  if (it == adaptors.end())
    return matchVar(v, pattern, env);

  return it->second.match(v, pattern, env);
}

static Value *build(Builder &builder, const Pattern *pattern, const Env &env) {
  auto it = adaptors.find((int) pattern->op);
  if (it == adaptors.end())
    return buildVar(builder, pattern, env);

  return it->second.build(builder, pattern, env);
}

template<class T>
bool matchEmptyImpl(Value *v, const Pattern *pattern, Env &env) {
  if (pattern->kind == Pattern::Imm)
    return false;

  if (matchVar(v, pattern, env))
    return true;

  auto op = v->def;
  if (op->getNumOperands() > 3)
    return false;

  if (pattern->op != OpKindOf<T>::value || (unsigned) pattern->size() != op->getNumOperands())
    return false;

  for (auto [i, pat] : data::enumerate(pattern->children)) {
    if (pat && !match(op->val(i), pat, env))
      return false;
  }
  return true;
}

template<class T>
Value *buildEmptyImpl(Builder &builder, const Pattern *pattern, const Env &env) {
  if (pattern->kind == Pattern::Var)
    return env.vals.at(pattern->name);

  assert(pattern->kind != Pattern::Imm);
  Value *operands[3];
  int i = 0;
  for (; i < 3; i++) {
    auto ch = pattern->children[i];
    if (!ch)
      break;

    operands[i] = build(builder, ch, env);
  }

  Op *op;
  if (strlen(pattern->tyname) == 0)
    op = builder.create<T>();
  else {
    auto ty = env.types.at(pattern->tyname);
    op = builder.create<T>(ty);
  }

  for (int j = 0; j < i; j++)
    op->pushOperand(operands[j]);
  return op->ret();
}

template<class T>
bool matchImmImpl(Value *v, const Pattern *pattern, Env &env) {
  if (matchVar(v, pattern, env))
    return true;

  Pattern pat(*pattern);

  auto size = pat.size();
  if (size < 1)
    return false;

  auto &last = pat.children[size - 1];
  if (last->kind != Pattern::Imm)
    return false;
  
  auto op = v->def;
  env.imms[last->name] = cast<T>(op)->value;
  last = nullptr;
  return matchEmptyImpl<T>(v, &pat, env);
}

#define binact(Ty, op) \
  { ActionKind::Ty, [](int x, int y) { return x op y; } },

int evaluate(const Pattern *pattern, const Env &env) {
  if (pattern->kind == Pattern::Imm)
    return env.imms.at(pattern->name);

  assert(pattern->kind == Pattern::Action);
  if (pattern->act == ActionKind::literal)
    return (unsigned long) pattern->children[0];

  const static std::map<ActionKind, std::function<int(int, int)>> binmap {
    action_list(binact)
  };
  return binmap.at(pattern->act)(
    evaluate(pattern->children[0], env),
    evaluate(pattern->children[1], env)
  );
}

template<class T>
Value *buildImmImpl(Builder &builder, const Pattern *pattern, const Env &env) {
  Pattern pat(*pattern);

  auto size = pat.size();
  auto &last = pat.children[size - 1];
  auto value = evaluate(last, env);
  last = nullptr;

  auto op = buildEmptyImpl<T>(builder, &pat, env)->def;
  cast<T>(op)->value = value;

  return op->ret();
}

#define adaptor_decl(Ty, infix) { (decltype(Op::id)) OpKind::Ty, OpAdaptor { match##infix##Impl<Ty>, build##infix##Impl<Ty> } },
#define empty_adaptor_decl(Ty) adaptor_decl(Ty, Empty)
#define imm_adaptor_decl(Ty) adaptor_decl(Ty, Imm)

Adaptors adaptors {
  empty_op_list(empty_adaptor_decl)
  imm_op_list(imm_adaptor_decl)
  adaptor_decl(Int64Op, Imm)
};

Value *Rule::build(Builder &builder) {
  if (!building || (pred && !(*pred)(env)))
    return nullptr;

  return ::build(builder, building, env);
}

bool Rule::match(Op *op) {
  if (!op || op->getNumResults() != 1)
    return false;

  return ::match(op->ret(), matching, env);
}

Op *Rule::rewrite(Op *op) {
  env.clear();
  if (!match(op))
    return nullptr;

  Builder builder;
  builder.setBefore(op);
  auto after = build(builder);
  if (!after)
    return nullptr;
  
  if (op->getNumResults() > 0)
    op->ret()->replaceAllUsesWith(after);
  op->erase();
  return after->def;
}

}
