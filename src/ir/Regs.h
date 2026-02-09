#ifndef REGS_H
#define REGS_H

#include <set>

#define regs_list(X) \
  /* x0 - x7: arguments */ \
  X(x0) \
  X(x1) \
  X(x2) \
  X(x3) \
  X(x4) \
  X(x5) \
  X(x6) \
  X(x7) \
  /* x8: indirect result (we don't need it) */ \
  X(x8) \
  /* x9 - x15: caller saved (temps) */ \
  X(x9) \
  X(x10) \
  X(x11) \
  X(x12) \
  X(x13) \
  X(x14) \
  X(x15) \
  /* x16 - x18: reserved. Avoid them. */ \
  X(x16) \
  X(x17) \
  X(x18) \
  /* x19 - x29: callee saved. (x29 can be `fp`) */ \
  X(x19) \
  X(x20) \
  X(x21) \
  X(x22) \
  X(x23) \
  X(x24) \
  X(x25) \
  X(x26) \
  X(x27) \
  X(x28) \
  X(x29) \
  /* x30: ra */ \
  X(x30) \
  /* x31: either sp or zero, based on context; we consider it as two separate ones */ \
  X(sp) \
  X(xzr) \
  fp_reg_list(X) \

#define fp_reg_list(X) \
  /* v0 - v7: arguments */ \
  X(v0) \
  X(v1) \
  X(v2) \
  X(v3) \
  X(v4) \
  X(v5) \
  X(v6) \
  X(v7) \
  /* v8 - v15: caller saved (temps) */ \
  X(v8) \
  X(v9) \
  X(v10) \
  X(v11) \
  X(v12) \
  X(v13) \
  X(v14) \
  X(v15) \
  /* v16 - v31: callee saved */ \
  X(v16) \
  X(v17) \
  X(v18) \
  X(v19) \
  X(v20) \
  X(v21) \
  X(v22) \
  X(v23) \
  X(v24) \
  X(v25) \
  X(v26) \
  X(v27) \
  X(v28) \
  X(v29) \
  X(v30) \
  X(v31)

namespace ir {

constexpr int FP = 0, INT = 1;

#define reg_decl(reg) reg,
enum Reg {
  unallocated,
  regs_list(reg_decl)
  reg_end
};

#define reg_name(reg) #reg,
constexpr const char *regnames[] = {
  "<no alloc>", regs_list(reg_name)
};

inline constexpr const char *regname(int t) {
  if (t >= reg_end || t < 0)
    return "<bad>";
  
  return regnames[t];
}

#define fp_reg(reg) case reg:
inline bool regbank(Reg reg) {
  switch (reg) {
  fp_reg_list(fp_reg)
    return FP;
  default:
    return INT;
  }
}


const Reg fargRegs[] = {
  v0, v1, v2, v3,
  v4, v5, v6, v7,
};
const Reg argRegs[] = {
  x0, x1, x2, x3,
  x4, x5, x6, x7,
};

const Reg scratch = x17;

// Order for leaf functions. Prioritize temporaries.
const Reg leafOrder[] = {
  x0, x1, x2, x3,
  x4, x5, x6, x7,

  x8, x9, x10, x11,
  x12, x13, x14, x15,
  x16,

  x19, x20, x21, x22,
  x23, x24, x25, x26,
  x27, x28, x29
};
// Order for non-leaf functions.
const Reg normalOrder[] = {
  x0, x1, x2, x3,
  x4, x5, x6, x7,

  x8, x9, x10, x11,
  x12, x13, x14, x15,
  x16,

  x19, x20, x21, x22,
  x23, x24, x25, x26,
  x27, x28, x29
};

// The same, but for floating point registers.
const Reg leafOrderf[] = {
  v0, v1, v2, v3,
  v4, v5, v6, v7,

  v8, v9, v10, v11,
  v12, v13, v14, v15,

  v16, v17, v18, v19,
  v20, v21, v22, v23,
  v24, v25, v26, v27,
  v28, v29, v30, v31,
};
// Order for non-leaf functions.
const Reg normalOrderf[] = {
  v0, v1, v2, v3,
  v4, v5, v6, v7,

  v8, v9, v10, v11,
  v12, v13, v14, v15,

  v16, v17, v18, v19,
  v20, v21, v22, v23,
  v24, v25, v26, v27,
  v28, v29, v30, v31,
};

const std::set<Reg> callerSaved = {
  x0, x1, x2, x3,
  x4, x5, x6, x7,

  x8, x9, x10, x11,
  x12, x13, x14, x15,
  x16, x17,

  v0, v1, v2, v3,
  v4, v5, v6, v7,

  v8, v9, v10, v11,
  v12, v13, v14, v15,
};

const std::set<Reg> calleeSaved = {
  x19, x20, x21, x22,
  x23, x24, x25, x26,
  x27, x28, x29, x30,

  v16, v17, v18, v19,
  v20, v21, v22, v23,
  v24, v25, v26, v27,
  v28, v29, v30, v31,
};

constexpr int regcnt = sizeof(leafOrder) / sizeof(leafOrder[0]);
constexpr int regcntf = sizeof(leafOrderf) / sizeof(leafOrderf[0]);

static_assert(regcnt == sizeof(normalOrder) / sizeof(normalOrder[0]));
static_assert(regcntf == sizeof(normalOrderf) / sizeof(normalOrderf[0]));

class Type;
int regbank(const Type *ty);

}

#undef reg_decl
#undef reg_name
#undef fp_reg
#endif
