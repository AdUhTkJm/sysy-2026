#ifndef MATCHER_H
#define MATCHER_H

#include "Builder.h"
#include <unordered_map>
#include <functional>
#include <optional>

namespace ir::match {

struct Env {
  std::unordered_map<std::string, Value*> vals;
  std::unordered_map<std::string, long> imms;
  std::unordered_map<std::string, const Type*> types;

  Env() { refillTypes(); }
  void clear();
  void refillTypes();
};

#define action_list(X) \
  X(add, +) X(sub, -) X(mul, *) X(div, /) X(mod, %) \
  X(lt, <) X(le, <=) X(eq, ==) X(ne, !=)

#define action_decl(Ty, ...) Ty, 
enum class ActionKind {
  action_list(action_decl)
  literal
};

#undef action_decl
extern const std::map<std::string, ActionKind> actionNames;

struct Pattern {
  enum { Var, Imm, Op, Action } kind;
  char tyname[4] {};
  union {
    char name[32];           // For Var or Imm
    struct {                 // For Op NOLINT
      const Pattern *children[3] {};
      union {
        ActionKind act;
        OpKind op;
      };
    };
  };

  static Arena arena;
  static void* operator new(size_t size) { return arena.allocate(size, alignof(Type)); }
  static void operator delete(void*) noexcept {}
  static void *operator new[](size_t) = delete;
  static void operator delete[](void*) noexcept = delete;

  Pattern(decltype(kind) k, std::string_view name);
  Pattern(OpKind kind);
  Pattern(ActionKind kind);

  static const Pattern *make(std::string_view str);
  int size() const;
};

struct OpAdaptor {
  bool (*match)(Value *v, const Pattern *pattern, Env &env);
  Value *(*build)(Builder &builder, const Pattern *pattern, const Env &env);
};

using Adaptors = std::unordered_map<std::remove_cv<decltype(Op::id)>::type, OpAdaptor>;

extern Adaptors adaptors;

class Rule {
  using Predicate = std::function<bool(const Env&)>;

  const Pattern *matching, *building = nullptr;
  Env env;
  std::optional<Predicate> pred;
  Builder builder;
public:
  Rule(const char *str): Rule(std::string_view(str)) {}
  Rule(std::string_view str): Rule(Pattern::make(str)) {}
  Rule(const Pattern *match): matching(match) {}
  Rule &operator>>(std::string_view str) { return *this >> Pattern::make(str); }
  Rule &operator>>(const Pattern *build) { building = build; return *this; }
  Rule &operator&(const Predicate &p) { pred = p; return *this; }

  bool match(Op *op);
  Value *build(Builder &builder);
  Op *rewrite(Op *op);
  void where(const Predicate &p) { pred = p; }
};

}

#endif
