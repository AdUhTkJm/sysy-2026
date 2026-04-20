#ifndef INTEGER_RELATION
#define INTEGER_RELATION

#include "Matrix.h"
#include <vector>
#include <iosfwd>
#include <optional>

namespace pres {

struct Space {
  unsigned domain, range, symbol, local;

  unsigned dimension() const { return domain + range + symbol + local; }
  unsigned getNumCols() const { return dimension() + 1; }

  bool compatible(Space other) const {
    return domain == other.domain && range == other.range && symbol == other.symbol;
  }

  void assertCompatible(Space other) const {
    assert(compatible(other));
  }
};

struct Constraint {
  using Int = Matrix::value_type;
  using Coeffs = std::unordered_map<unsigned, Int>;

  Coeffs coeffs;
  Int constant;
  enum {
    Incomplete, Eq, Ineq
  } status;

  Constraint flipped() const;
  Constraint operator>=(Int c) const { return { coeffs, constant - c, Ineq }; }
  Constraint operator>(Int c) const { return { coeffs, constant - c - 1, Ineq }; }
  Constraint operator<=(Int c) const { auto w = flipped(); w.constant += c; w.status = Ineq; return w; }
  Constraint operator<(Int c) const { auto w = flipped(); w.constant += c + 1; w.status = Ineq; return w; }
  Constraint operator==(Int c) const { return { coeffs, constant - c, Eq }; }

  Constraint operator+(const Constraint &other) const;
  Constraint operator-(const Constraint &other) const;
};

struct Var {
  Constraint operator[](unsigned i) const { return { { { i, 1 } }, 0, Constraint::Incomplete }; }
} const extern var;

class PresburgerRelation;
class IntegerRelation {
public:
  Matrix eqs, ineqs;
  Space space;

  using Int = Matrix::value_type;
  using Row = Matrix::Row;

  IntegerRelation(Space space, const Matrix &equalities, const Matrix &inequalities);
  IntegerRelation(Space space = {}, const std::vector<Row> &eqs = {}, const std::vector<Row> &ineqs = {});
  IntegerRelation(Space space, const IntegerRelation::Row &point);

  unsigned getNumCols() const { return space.dimension() + 1; }
  unsigned getNumEqualities() const { return eqs.getNumRows(); }
  unsigned getNumInequalities() const { return ineqs.getNumRows(); }
  unsigned dimension() const { return space.dimension(); }

  void addEquality(const Row &row) { eqs.appendRow(row); }
  void addInequality(const Row &row) { ineqs.appendRow(row); }
  void add(const Constraint &constr);

  bool isUniverse() const { return eqs.getNumRows() == 0 && ineqs.getNumRows() == 0; }
  bool isObviouslyEmpty() const;
  bool isEmpty() const;
  bool containsPoint(const IntegerRelation::Row &point) const { return !intersect(IntegerRelation(space, point)).isEmpty(); }

  std::optional<Row> findSamplePoint() const;

  struct Simplex {
    FracMatrix tableau;
    std::vector<unsigned> basis;
    bool feasible;
  };
  Simplex simplex(const Row &target) const;
  std::optional<Fraction> getRationalMin(const Row &target) const;

  void simplify();
  void gcdTightenInequalities();
  void normalize(); // Divide each row by gcd.
  void dump(std::ostream &os = std::cerr) const;

  static IntegerRelation intersection(const IntegerRelation &lhs, const IntegerRelation &rhs);
  static PresburgerRelation setUnion(const IntegerRelation &lhs, const IntegerRelation &rhs);
  static PresburgerRelation setDifference(const IntegerRelation &lhs, const IntegerRelation &rhs);

  IntegerRelation intersect(const IntegerRelation &other) const { return intersection(*this, other); }
};

class PresburgerRelation {
public:
  std::vector<IntegerRelation> disjuncts;
  using Row = IntegerRelation::Row;

  PresburgerRelation() = default;
  PresburgerRelation(const IntegerRelation &disjunct): disjuncts({ disjunct }) {}
  PresburgerRelation(const std::vector<IntegerRelation> &disjuncts): disjuncts(disjuncts) {}
  PresburgerRelation(std::vector<IntegerRelation> &&disjuncts): disjuncts(std::move(disjuncts)) {}

  bool isEmpty() const;
  bool containsPoint(const Row &row) const;
  void simplify();
  void dump(std::ostream &os) const;

  unsigned getNumDisjuncts() const { return disjuncts.size(); }

  static PresburgerRelation intersection(const PresburgerRelation &lhs, const PresburgerRelation &rhs);
  static PresburgerRelation setUnion(const PresburgerRelation &lhs, const PresburgerRelation &rhs);
  static PresburgerRelation setDifference(const PresburgerRelation &lhs, const PresburgerRelation &rhs);

  PresburgerRelation intersect(const PresburgerRelation &other) const { return intersection(*this, other); }
  PresburgerRelation unite(const PresburgerRelation &other) const { return setUnion(*this, other); }
  PresburgerRelation subtract(const PresburgerRelation &other) const { return setDifference(*this, other); }

  PresburgerRelation &operator+=(const PresburgerRelation &other);
};

inline PresburgerRelation operator+(const IntegerRelation &l, const IntegerRelation &r) {
  return IntegerRelation::setUnion(l, r);
}

inline PresburgerRelation operator-(const IntegerRelation &l, const IntegerRelation &r) {
  return IntegerRelation::setDifference(l, r);
}

inline PresburgerRelation operator+(const PresburgerRelation &l, const PresburgerRelation &r) {
  return PresburgerRelation::setUnion(l, r);
}

inline PresburgerRelation operator-(const PresburgerRelation &l, const PresburgerRelation &r) {
  return PresburgerRelation::setDifference(l, r);
}

inline bool operator==(const IntegerRelation &l, const IntegerRelation &r) {
  return (l - r).isEmpty() && (r - l).isEmpty();
}

inline bool operator!=(const IntegerRelation &l, const IntegerRelation &r) {
  return !(l == r);
}

inline bool operator==(const PresburgerRelation &l, const PresburgerRelation &r) {
  return (l - r).isEmpty() && (r - l).isEmpty();
}

inline bool operator!=(const PresburgerRelation &l, const PresburgerRelation &r) {
  return !(l == r);
}

std::ostream &operator<<(std::ostream &os, const IntegerRelation &relation);
std::ostream &operator<<(std::ostream &os, const PresburgerRelation &relation);

}

#endif
