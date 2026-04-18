#ifndef INTEGER_RELATION
#define INTEGER_RELATION

#include "Matrix.h"
#include <vector>
#include <iosfwd>

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

class PresburgerRelation;
class IntegerRelation {
  Matrix eqs, ineqs;
  Space space;
public:
  using Int = Matrix::value_type;
  using Row = std::vector<Int>;

  IntegerRelation(Space space, const Matrix &equalities, const Matrix &inequalities);
  IntegerRelation(Space space = {}, const std::vector<Row> &eqs = {}, const std::vector<Row> &ineqs = {});

  const Space &getSpace() const { return space; }
  unsigned getNumCols() const { return space.dimension() + 1; }
  unsigned getNumEqualities() const { return eqs.rowCount(); }
  unsigned getNumInequalities() const { return ineqs.rowCount(); }

  const Matrix &equalities() const { return eqs; }
  Matrix &equalities() { return eqs; }

  const Matrix &inequalities() const { return ineqs; }
  Matrix &inequalities() { return ineqs; }

  void addEquality(const std::vector<Int> &row);
  void addInequality(const std::vector<Int> &row);

  bool isUniverse() const { return eqs.rowCount() == 0 && ineqs.rowCount() == 0; }
  bool isObviouslyEmpty() const;
  bool isEmpty() const;

  void normalize();
  void dump(std::ostream &os) const;

  static IntegerRelation intersection(const IntegerRelation &lhs, const IntegerRelation &rhs);
  static PresburgerRelation setUnion(const IntegerRelation &lhs, const IntegerRelation &rhs);
  static PresburgerRelation setDifference(const IntegerRelation &lhs, const IntegerRelation &rhs);

  IntegerRelation intersect(const IntegerRelation &other) const { return intersection(*this, other); }
  bool empty() const { return isEmpty(); }
  PresburgerRelation subtract(const IntegerRelation &other) const;
};

class PresburgerRelation {
  std::vector<IntegerRelation> disjuncts;
public:
  PresburgerRelation() = default;
  PresburgerRelation(const IntegerRelation &disjunct): disjuncts({ disjunct }) {}
  PresburgerRelation(const std::vector<IntegerRelation> &disjuncts): disjuncts(disjuncts) {}
  PresburgerRelation(std::vector<IntegerRelation> &&disjuncts): disjuncts(std::move(disjuncts)) {}

  const std::vector<IntegerRelation> &getDisjuncts() const { return disjuncts; }
  std::vector<IntegerRelation> &getDisjuncts() { return disjuncts; }

  bool isEmpty() const;
  bool empty() const { return isEmpty(); }
  void normalize();
  void dump(std::ostream &os) const;

  static PresburgerRelation intersection(const PresburgerRelation &lhs, const PresburgerRelation &rhs);
  static PresburgerRelation setUnion(const PresburgerRelation &lhs, const PresburgerRelation &rhs);
  static PresburgerRelation setDifference(const PresburgerRelation &lhs, const PresburgerRelation &rhs);

  PresburgerRelation intersect(const PresburgerRelation &other) const { return intersection(*this, other); }
  PresburgerRelation unite(const PresburgerRelation &other) const { return setUnion(*this, other); }
  PresburgerRelation subtract(const PresburgerRelation &other) const { return setDifference(*this, other); }
};

inline PresburgerRelation operator+(const IntegerRelation &l, const IntegerRelation &r) {
  return IntegerRelation::setUnion(l, r);
}

inline PresburgerRelation operator-(const IntegerRelation &l, const IntegerRelation &r) {
  return IntegerRelation::setDifference(l, r);
}

std::ostream &operator<<(std::ostream &os, const IntegerRelation &relation);
std::ostream &operator<<(std::ostream &os, const PresburgerRelation &relation);

}

#endif
