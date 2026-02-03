#ifndef REGS_H
#define REGS_H

#include <set>
#include "../../ir/OpBase.h"

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

namespace opt {

#define reg_decl(reg) reg,
enum Reg {
  regs_list(reg_decl)
  End
};

#define reg_name(reg) #reg,
constexpr const char *regnames[] = {
  regs_list(reg_name)
};

inline constexpr const char *regname(int t) {
  if (t >= End || t < 0)
    return "<unknown>";
  
  return regnames[t];
}

#define fp_reg(reg) case reg:
inline bool isFP(Reg reg) {
  switch (reg) {
  fp_reg_list(fp_reg)
    return true;
  default:
    return false;
  }
}


const Reg fargRegs[] = {
  Reg::v0, Reg::v1, Reg::v2, Reg::v3,
  Reg::v4, Reg::v5, Reg::v6, Reg::v7,
};
const Reg argRegs[] = {
  Reg::x0, Reg::x1, Reg::x2, Reg::x3,
  Reg::x4, Reg::x5, Reg::x6, Reg::x7,
};

// We use dedicated registers as the "spill" registers, for simplicity.
const Reg spillReg = Reg::x28;
const Reg spillReg2 = Reg::x15;
const Reg fspillReg = Reg::v31;
const Reg fspillReg2 = Reg::v15;

// Order for leaf functions. Prioritize temporaries.
const Reg leafOrder[] = {
  Reg::x0, Reg::x1, Reg::x2, Reg::x3,
  Reg::x4, Reg::x5, Reg::x6, Reg::x7,

  Reg::x8, Reg::x9, Reg::x10, Reg::x11,
  Reg::x12, Reg::x13, Reg::x14,
  Reg::x16, Reg::x17,

  Reg::x19, Reg::x20, Reg::x21, Reg::x22,
  Reg::x23, Reg::x24, Reg::x25, Reg::x26,
  Reg::x27,
};
// Order for non-leaf functions.
const Reg normalOrder[] = {
  Reg::x0, Reg::x1, Reg::x2, Reg::x3,
  Reg::x4, Reg::x5, Reg::x6, Reg::x7,

  Reg::x8, Reg::x9, Reg::x10, Reg::x11,
  Reg::x12, Reg::x13, Reg::x14,
  Reg::x16, Reg::x17,

  Reg::x19, Reg::x20, Reg::x21, Reg::x22,
  Reg::x23, Reg::x24, Reg::x25, Reg::x26,
  Reg::x27,
};

// The same, but for floating point registers.
const Reg leafOrderf[] = {
  Reg::v0, Reg::v1, Reg::v2, Reg::v3,
  Reg::v4, Reg::v5, Reg::v6, Reg::v7,

  Reg::v8, Reg::v9, Reg::v10, Reg::v11,
  Reg::v12, Reg::v13, Reg::v14,

  Reg::v16, Reg::v17, Reg::v18,
  Reg::v19, Reg::v20, Reg::v21, Reg::v22,
  Reg::v23, Reg::v24, Reg::v25, Reg::v26,
  Reg::v27, Reg::v28, Reg::v29,
};
// Order for non-leaf functions.
const Reg normalOrderf[] = {
  Reg::v0, Reg::v1, Reg::v2, Reg::v3,
  Reg::v4, Reg::v5, Reg::v6, Reg::v7,

  Reg::v8, Reg::v9, Reg::v10, Reg::v11,
  Reg::v12, Reg::v13, Reg::v14,

  Reg::v16, Reg::v17, Reg::v18,
  Reg::v19, Reg::v20, Reg::v21, Reg::v22,
  Reg::v23, Reg::v24, Reg::v25, Reg::v26,
  Reg::v27, Reg::v28, Reg::v29,
};

const std::set<Reg> callerSaved = {
  Reg::x0, Reg::x1, Reg::x2, Reg::x3,
  Reg::x4, Reg::x5, Reg::x6, Reg::x7,

  Reg::x8, Reg::x9, Reg::x10, Reg::x11,
  Reg::x12, Reg::x13, Reg::x14, Reg::x15,
  Reg::x16, Reg::x17,

  Reg::v0, Reg::v1, Reg::v2, Reg::v3,
  Reg::v4, Reg::v5, Reg::v6, Reg::v7,

  Reg::v8, Reg::v9, Reg::v10, Reg::v11,
  Reg::v12, Reg::v13, Reg::v14, Reg::v15,
};

const std::set<Reg> calleeSaved = {
  Reg::x19, Reg::x20, Reg::x21, Reg::x22,
  Reg::x23, Reg::x24, Reg::x25, Reg::x26,
  Reg::x27, Reg::x28,

  Reg::v16, Reg::v17, Reg::v18,
  Reg::v19, Reg::v20, Reg::v21, Reg::v22,
  Reg::v23, Reg::v24, Reg::v25, Reg::v26,
  Reg::v27, Reg::v28, Reg::v29, Reg::v30,
};

constexpr int leafRegCnt = 26;
constexpr int leafRegCntf = 29;
constexpr int normalRegCnt = 26;
constexpr int normalRegCntf = 29;

constexpr int FP = 0, INT = 1;

inline int regbank(const ir::Type *ty) {
  if (ty == ir::f32 || ty == ir::vi4 || ty == ir::vf4)
    return FP;
  return INT;
}

}

#undef reg_decl
#undef reg_name
#undef fp_reg
#endif
