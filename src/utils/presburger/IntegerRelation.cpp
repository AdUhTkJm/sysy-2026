#include "IntegerRelation.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <optional>
#include <ostream>
#include <utility>

namespace pres {

namespace {

using Int = IntegerRelation::Int;
using Row = std::vector<Int>;

void assertColumnCount(const Matrix &matrix, unsigned cols) {
  assert(matrix.colCount() == cols);
}

void appendRow(Matrix &matrix, const std::vector<Int> &row) {
  assert(matrix.colCount() == row.size());
  const unsigned last = matrix.rowCount();
  matrix.resize(last + 1, matrix.colCount());
  for (unsigned c = 0; c < row.size(); ++c)
    matrix(last, c) = row[c];
}

std::vector<Int> extractRow(const Matrix &matrix, unsigned row) {
  std::vector<Int> result(matrix.colCount());
  for (unsigned c = 0; c < matrix.colCount(); ++c)
    result[c] = matrix(row, c);
  return result;
}

void divideByGcd(std::vector<Int> &row) {
  Int gcd = 0;
  for (Int value : row)
    gcd = pres::gcd(gcd, static_cast<Int>(abs(value)));

  if (gcd <= 1)
    return;

  for (Int &value : row)
    value /= gcd;
}

void canonicalizeEqualityRow(std::vector<Int> &row) {
  divideByGcd(row);
  for (Int value : row) {
    if (value == 0)
      continue;
    if (value < 0) {
      for (Int &entry : row)
        entry = -entry;
    }
    return;
  }
}

bool rowLess(const std::vector<Int> &lhs, const std::vector<Int> &rhs) {
  return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

void normalizeRows(Matrix &matrix, bool equality) {
  std::vector<std::vector<Int>> rows;
  rows.reserve(matrix.rowCount());

  for (unsigned r = 0; r < matrix.rowCount(); ++r) {
    auto row = extractRow(matrix, r);
    if (equality)
      canonicalizeEqualityRow(row);
    rows.push_back(std::move(row));
  }

  std::sort(rows.begin(), rows.end(), rowLess);
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

  matrix.resize(rows.size(), matrix.colCount());
  for (unsigned r = 0; r < rows.size(); ++r) {
    for (unsigned c = 0; c < matrix.colCount(); ++c)
      matrix(r, c) = rows[r][c];
  }
}

std::vector<Int> shiftedLowerBound(const std::vector<Int> &row) {
  auto result = row;
  --result.back();
  return result;
}

std::vector<Int> strictNegation(const std::vector<Int> &row) {
  auto result = row;
  for (Int &value : result)
    value = -value;
  --result.back();
  return result;
}

bool areEqualRelations(const IntegerRelation &lhs, const IntegerRelation &rhs) {
  if (lhs.getNumCols() != rhs.getNumCols() ||
      lhs.getNumEqualities() != rhs.getNumEqualities() ||
      lhs.getNumInequalities() != rhs.getNumInequalities())
    return false;

  for (unsigned r = 0; r < lhs.getNumEqualities(); ++r)
    for (unsigned c = 0; c < lhs.getNumCols(); ++c)
      if (lhs.equalities()(r, c) != rhs.equalities()(r, c))
        return false;

  for (unsigned r = 0; r < lhs.getNumInequalities(); ++r)
    for (unsigned c = 0; c < lhs.getNumCols(); ++c)
      if (lhs.inequalities()(r, c) != rhs.inequalities()(r, c))
        return false;

  return true;
}

bool rationallyFeasible(unsigned dimension, std::vector<Row> inequalities) {
  for (unsigned var = 0; var < dimension; ++var) {
    std::vector<Row> positive;
    std::vector<Row> negative;
    std::vector<Row> zero;

    for (auto &row : inequalities) {
      if (row[var] > 0)
        positive.push_back(row);
      else if (row[var] < 0)
        negative.push_back(row);
      else
        zero.push_back(row);
    }

    std::vector<Row> next;
    next.reserve(zero.size() + positive.size() * negative.size());

    for (auto &row : zero) {
      Row projected;
      projected.reserve(row.size() - 1);
      for (unsigned c = 0; c < row.size(); ++c)
        if (c != var)
          projected.push_back(row[c]);
      divideByGcd(projected);
      next.push_back(std::move(projected));
    }

    for (auto &p : positive) {
      for (auto &n : negative) {
        const Int posCoeff = p[var];
        const Int negCoeff = -n[var];

        Row combined;
        combined.reserve(p.size() - 1);
        for (unsigned c = 0; c < p.size(); ++c) {
          if (c == var)
            continue;
          combined.push_back(negCoeff * p[c] + posCoeff * n[c]);
        }
        divideByGcd(combined);
        next.push_back(std::move(combined));
      }
    }

    std::sort(next.begin(), next.end());
    next.erase(std::unique(next.begin(), next.end()), next.end());
    inequalities = std::move(next);
  }

  for (const auto &row : inequalities)
    if (!row.empty() && row.back() < 0)
      return false;

  return true;
}

Int floorDiv(Int num, Int den) {
  assert(den > 0);
  if (num >= 0)
    return num / den;
  return -(((-num) + den - 1) / den);
}

Int ceilDiv(Int num, Int den) {
  assert(den > 0);
  return -floorDiv(-num, den);
}

bool singleVariableIntegerFeasible(const IntegerRelation &relation) {
  assert(relation.getSpace().dimension() == 1);

  bool fixed = false;
  Int value = 0;
  Int lower = std::numeric_limits<Int>::min() / 4;
  Int upper = std::numeric_limits<Int>::max() / 4;

  for (unsigned r = 0; r < relation.getNumEqualities(); ++r) {
    const Int a = relation.equalities()(r, 0);
    const Int c = relation.equalities()(r, 1);

    if (a == 0) {
      if (c != 0)
        return false;
      continue;
    }

    if ((-c) % a != 0)
      return false;

    const Int candidate = (-c) / a;
    if (fixed && candidate != value)
      return false;
    fixed = true;
    value = candidate;
  }

  for (unsigned r = 0; r < relation.getNumInequalities(); ++r) {
    const Int a = relation.inequalities()(r, 0);
    const Int c = relation.inequalities()(r, 1);

    if (a == 0) {
      if (c < 0)
        return false;
      continue;
    }

    if (a > 0)
      lower = std::max(lower, ceilDiv(-c, a));
    else
      upper = std::min(upper, floorDiv(c, -a));
  }

  if (fixed)
    return lower <= value && value <= upper;

  return lower <= upper;
}

bool hasIntegerEqualitySolution(const Row &row) {
  Int gcd = 0;
  for (unsigned c = 0; c + 1 < row.size(); ++c)
    gcd = pres::gcd(gcd, static_cast<Int>(abs(row[c])));

  const Int constant = row.back();
  if (gcd == 0)
    return constant == 0;
  return constant % gcd == 0;
}

bool obviouslyEmpty(const std::vector<Row> &eqs, const std::vector<Row> &ineqs) {
  for (const auto &row : eqs) {
    bool allZero = true;
    for (unsigned c = 0; c + 1 < row.size(); ++c) {
      if (row[c] != 0) {
        allZero = false;
        break;
      }
    }
    if ((allZero && row.back() != 0) || !hasIntegerEqualitySolution(row))
      return true;
  }

  for (const auto &row : ineqs) {
    bool allZero = true;
    for (unsigned c = 0; c + 1 < row.size(); ++c) {
      if (row[c] != 0) {
        allZero = false;
        break;
      }
    }
    if (allZero && row.back() < 0)
      return true;
  }

  return false;
}

struct BigRelation {
  unsigned dimension;
  std::vector<Row> eqs;
  std::vector<Row> ineqs;
};

BigRelation toRelation(const IntegerRelation &relation) {
  BigRelation result { relation.getSpace().dimension(), {}, {} };
  result.eqs.reserve(relation.getNumEqualities());
  result.ineqs.reserve(relation.getNumInequalities());

  for (unsigned r = 0; r < relation.getNumEqualities(); ++r) {
    auto row = extractRow(relation.equalities(), r);
    canonicalizeEqualityRow(row);
    result.eqs.push_back(std::move(row));
  }
  for (unsigned r = 0; r < relation.getNumInequalities(); ++r) {
    auto row = extractRow(relation.inequalities(), r);
    divideByGcd(row);
    result.ineqs.push_back(std::move(row));
  }

  return result;
}

void dedupRows(std::vector<Row> &rows) {
  std::sort(rows.begin(), rows.end());
  rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
}

std::optional<unsigned> findEqualityWithNonZero(const BigRelation &relation, unsigned var) {
  for (unsigned i = 0; i < relation.eqs.size(); ++i)
    if (relation.eqs[i][var] != 0)
      return i;
  return std::nullopt;
}

void eliminateVar(BigRelation &relation, unsigned var) {
  assert(var < relation.dimension);
  const unsigned cols = relation.dimension + 1;

  if (auto maybeEq = findEqualityWithNonZero(relation, var)) {
    Row pivot = relation.eqs[*maybeEq];
    if (pivot[var] < 0) {
      for (Int &entry : pivot)
        entry = -entry;
    }
    const Int a = pivot[var];

    std::vector<Row> newEqs;
    newEqs.reserve(relation.eqs.size());
    for (unsigned i = 0; i < relation.eqs.size(); ++i) {
      if (i == *maybeEq)
        continue;

      const Int coeff = relation.eqs[i][var];
      Row row(cols);
      for (unsigned c = 0; c < cols; ++c)
        row[c] = a * relation.eqs[i][c] - coeff * pivot[c];
      row.erase(row.begin() + var);
      canonicalizeEqualityRow(row);
      newEqs.push_back(std::move(row));
    }

    std::vector<Row> newIneqs;
    newIneqs.reserve(relation.ineqs.size());
    for (const Row &ineq : relation.ineqs) {
      const Int coeff = ineq[var];
      Row row(cols);
      for (unsigned c = 0; c < cols; ++c)
        row[c] = a * ineq[c] - coeff * pivot[c];
      row.erase(row.begin() + var);
      divideByGcd(row);
      newIneqs.push_back(std::move(row));
    }

    relation.eqs = std::move(newEqs);
    relation.ineqs = std::move(newIneqs);
    --relation.dimension;
    dedupRows(relation.eqs);
    dedupRows(relation.ineqs);
    return;
  }

  std::vector<Row> zeroRows;
  std::vector<Row> positiveRows;
  std::vector<Row> negativeRows;

  for (auto row : relation.ineqs) {
    if (row[var] == 0) {
      row.erase(row.begin() + var);
      divideByGcd(row);
      zeroRows.push_back(std::move(row));
    } else if (row[var] > 0) {
      positiveRows.push_back(std::move(row));
    } else {
      negativeRows.push_back(std::move(row));
    }
  }

  std::vector<Row> next = std::move(zeroRows);
  next.reserve(next.size() + positiveRows.size() * negativeRows.size());

  for (const Row &p : positiveRows) {
    for (const Row &n : negativeRows) {
      const Int pc = p[var];
      const Int nc = -n[var];
      Row row;
      row.reserve(cols - 1);
      for (unsigned c = 0; c < cols; ++c) {
        if (c == var)
          continue;
        row.push_back(nc * p[c] + pc * n[c]);
      }
      divideByGcd(row);
      next.push_back(std::move(row));
    }
  }

  for (auto &row : relation.eqs) {
    row.erase(row.begin() + var);
    canonicalizeEqualityRow(row);
  }

  relation.ineqs = std::move(next);
  --relation.dimension;
  dedupRows(relation.eqs);
  dedupRows(relation.ineqs);
}

struct IntegerInterval {
  bool hasLower = false;
  bool hasUpper = false;
  Int lower = 0;
  Int upper = 0;
};

std::optional<IntegerInterval> projectDirectionInterval(const BigRelation &relation,
                                                        const Row &direction) {
  assert(direction.size() == relation.dimension);

  BigRelation projected = relation;
  ++projected.dimension;

  for (auto &row : projected.eqs)
    row.insert(row.end() - 1, 0);
  for (auto &row : projected.ineqs)
    row.insert(row.end() - 1, 0);

  Row tie(projected.dimension + 1, 0);
  tie[projected.dimension - 1] = 1;
  for (unsigned i = 0; i < direction.size(); ++i)
    tie[i] = -direction[i];
  projected.eqs.push_back(tie);

  for (unsigned i = 0; i < relation.dimension; ++i)
    eliminateVar(projected, 0);

  if (obviouslyEmpty(projected.eqs, projected.ineqs))
    return std::nullopt;

  assert(projected.dimension == 1);
  IntegerInterval interval;

  for (const Row &eq : projected.eqs) {
    const Int a = eq[0];
    const Int c = eq[1];

    if (a == 0) {
      if (c != 0)
        return std::nullopt;
      continue;
    }
    if ((-c) % a != 0)
      return std::nullopt;

    const Int value = (-c) / a;
    if (interval.hasLower && interval.lower != value)
      return std::nullopt;
    if (interval.hasUpper && interval.upper != value)
      return std::nullopt;

    interval.hasLower = interval.hasUpper = true;
    interval.lower = interval.upper = value;
  }

  for (const Row &ineq : projected.ineqs) {
    const Int a = ineq[0];
    const Int c = ineq[1];

    if (a == 0) {
      if (c < 0)
        return std::nullopt;
      continue;
    }

    if (a > 0) {
      const Int bound = ceilDiv(-c, a);
      if (!interval.hasLower || bound > interval.lower) {
        interval.hasLower = true;
        interval.lower = bound;
      }
    } else {
      const Int bound = floorDiv(c, -a);
      if (!interval.hasUpper || bound < interval.upper) {
        interval.hasUpper = true;
        interval.upper = bound;
      }
    }
  }

  if (interval.hasLower && interval.hasUpper && interval.lower > interval.upper)
    return std::nullopt;

  return interval;
}

BigRelation addDirectionEquality(BigRelation relation, const Row &direction, Int value) {
  Row row(relation.dimension + 1, 0);
  for (unsigned i = 0; i < relation.dimension; ++i)
    row[i] = direction[i];
  row.back() = -value;
  canonicalizeEqualityRow(row);
  relation.eqs.push_back(std::move(row));
  dedupRows(relation.eqs);
  return relation;
}

std::vector<Row> identityBasis(unsigned n) {
  std::vector<Row> basis(n, Row(n, 0));
  for (unsigned i = 0; i < n; ++i)
    basis[i][i] = 1;
  return basis;
}

void reduceBasis(const BigRelation &relation, std::vector<Row> &basis) {
  if (basis.size() < 2)
    return;

  for (unsigned i = 0; i + 1 < basis.size(); ++i) {
    auto best = projectDirectionInterval(relation, basis[i + 1]);
    Int bestWidth = (!best || !best->hasLower || !best->hasUpper)
      ? std::numeric_limits<Int>::max()
      : best->upper - best->lower;
    Row bestDir = basis[i + 1];

    for (Int u = -2; u <= 2; ++u) {
      if (u == 0)
        continue;
      Row candidate = basis[i + 1];
      for (unsigned c = 0; c < candidate.size(); ++c)
        candidate[c] += u * basis[i][c];

      auto interval = projectDirectionInterval(relation, candidate);
      if (!interval || !interval->hasLower || !interval->hasUpper)
        continue;

      const Int width = interval->upper - interval->lower;
      if (width < bestWidth) {
        bestWidth = width;
        bestDir = std::move(candidate);
      }
    }

    basis[i + 1] = std::move(bestDir);
  }
}

bool searchIntegerPoint(BigRelation relation, std::vector<Row> basis) {
  dedupRows(relation.eqs);
  dedupRows(relation.ineqs);

  if (obviouslyEmpty(relation.eqs, relation.ineqs))
    return false;

  if (relation.dimension == 0)
    return true;

  if (!rationallyFeasible(relation.dimension, [&] {
        std::vector<Row> inequalities = relation.ineqs;
        inequalities.reserve(relation.ineqs.size() + 2 * relation.eqs.size());
        for (const Row &eq : relation.eqs) {
          inequalities.push_back(eq);
          Row neg = eq;
          for (Int &entry : neg)
            entry = -entry;
          inequalities.push_back(std::move(neg));
        }
        return inequalities;
      }()))
    return false;

  reduceBasis(relation, basis);

  std::optional<unsigned> bestIndex;
  IntegerInterval bestInterval;
  Int bestWidth = std::numeric_limits<Int>::max();
  bool allSingleton = true;
  bool hasPositiveWidthChoice = false;

  for (unsigned i = 0; i < basis.size(); ++i) {
    auto interval = projectDirectionInterval(relation, basis[i]);
    if (!interval)
      return false;
    if (!interval->hasLower || !interval->hasUpper) {
      allSingleton = false;
      continue;
    }

    const Int width = interval->upper - interval->lower;
    if (width != 0)
      allSingleton = false;
    if (width > 0 && width < bestWidth) {
      hasPositiveWidthChoice = true;
      bestWidth = width;
      bestIndex = i;
      bestInterval = *interval;
    }
  }

  if (allSingleton)
    return true;

  if (!hasPositiveWidthChoice)
    return true;

  for (Int value = bestInterval.lower; value <= bestInterval.upper; ++value) {
    if (searchIntegerPoint(addDirectionEquality(relation, basis[*bestIndex], value), basis))
      return true;
  }

  return false;
}

} // namespace


IntegerRelation::IntegerRelation(Space s, const std::vector<Row> &eqs, const std::vector<Row> &ineqs):
  eqs(0, s.getNumCols()), ineqs(0, s.getNumCols()), space(s) {
  for (const auto &row : eqs)
    addEquality(row);
  for (const auto &row : ineqs)
    addInequality(row);
  normalize();
}

IntegerRelation::IntegerRelation(Space space, const Matrix &equalities, const Matrix &inequalities):
  eqs(equalities), ineqs(inequalities), space(space) {
  assertColumnCount(eqs, getNumCols());
  assertColumnCount(ineqs, getNumCols());
}

void IntegerRelation::addEquality(const std::vector<Int> &row) {
  assert(row.size() == getNumCols());
  appendRow(eqs, row);
}

void IntegerRelation::addInequality(const std::vector<Int> &row) {
  assert(row.size() == getNumCols());
  appendRow(ineqs, row);
}

bool IntegerRelation::isObviouslyEmpty() const {
  for (unsigned r = 0; r < eqs.rowCount(); ++r) {
    bool allZero = true;
    for (unsigned c = 0; c + 1 < eqs.colCount(); ++c) {
      if (eqs(r, c) != 0) {
        allZero = false;
        break;
      }
    }
    if (allZero && eqs(r, eqs.colCount() - 1) != 0)
      return true;
  }

  for (unsigned r = 0; r < ineqs.rowCount(); ++r) {
    bool allZero = true;
    for (unsigned c = 0; c + 1 < ineqs.colCount(); ++c) {
      if (ineqs(r, c) != 0) {
        allZero = false;
        break;
      }
    }
    if (allZero && ineqs(r, ineqs.colCount() - 1) < 0)
      return true;
  }

  return false;
}

bool IntegerRelation::isEmpty() const {
  if (isObviouslyEmpty())
    return true;

  if (space.dimension() == 0)
    return false;

  IntegerRelation normalized = *this;
  normalized.normalize();
  if (normalized.isObviouslyEmpty())
    return true;

  if (normalized.getSpace().dimension() == 1)
    return !singleVariableIntegerFeasible(normalized);

  BigRelation relation = toRelation(normalized);
  return !searchIntegerPoint(std::move(relation), identityBasis(normalized.getSpace().dimension()));
}

void IntegerRelation::normalize() {
  normalizeRows(eqs, true);
  normalizeRows(ineqs, false);
}

void IntegerRelation::dump(std::ostream &os) const {
  os << "space=(" << space.domain << ", " << space.range << ", "
     << space.symbol << ", " << space.local << ")\n";

  os << "eqs:\n";
  for (unsigned r = 0; r < eqs.rowCount(); ++r) {
    os << "  [";
    for (unsigned c = 0; c < eqs.colCount(); ++c)
      os << eqs(r, c) << (c + 1 == eqs.colCount() ? "" : ", ");
    os << "] == 0\n";
  }

  os << "ineqs:\n";
  for (unsigned r = 0; r < ineqs.rowCount(); ++r) {
    os << "  [";
    for (unsigned c = 0; c < ineqs.colCount(); ++c)
      os << ineqs(r, c) << (c + 1 == ineqs.colCount() ? "" : ", ");
    os << "] >= 0\n";
  }
}

IntegerRelation IntegerRelation::intersection(const IntegerRelation &lhs, const IntegerRelation &rhs) {
  lhs.space.assertCompatible(rhs.space);
  assert(lhs.space.local == rhs.space.local);

  IntegerRelation result(lhs.space);
  result.eqs.resize(lhs.eqs.rowCount() + rhs.eqs.rowCount(), lhs.getNumCols());
  result.ineqs.resize(lhs.ineqs.rowCount() + rhs.ineqs.rowCount(), lhs.getNumCols());

  unsigned out = 0;
  for (unsigned r = 0; r < lhs.eqs.rowCount(); ++r, ++out)
    for (unsigned c = 0; c < lhs.getNumCols(); ++c)
      result.eqs(out, c) = lhs.eqs(r, c);
  for (unsigned r = 0; r < rhs.eqs.rowCount(); ++r, ++out)
    for (unsigned c = 0; c < rhs.getNumCols(); ++c)
      result.eqs(out, c) = rhs.eqs(r, c);

  out = 0;
  for (unsigned r = 0; r < lhs.ineqs.rowCount(); ++r, ++out)
    for (unsigned c = 0; c < lhs.getNumCols(); ++c)
      result.ineqs(out, c) = lhs.ineqs(r, c);
  for (unsigned r = 0; r < rhs.ineqs.rowCount(); ++r, ++out)
    for (unsigned c = 0; c < rhs.getNumCols(); ++c)
      result.ineqs(out, c) = rhs.ineqs(r, c);

  result.normalize();
  return result;
}

PresburgerRelation IntegerRelation::setUnion(const IntegerRelation &lhs, const IntegerRelation &rhs) {
  lhs.space.assertCompatible(rhs.space);
  assert(lhs.space.local == rhs.space.local);

  IntegerRelation left = lhs;
  IntegerRelation right = rhs;
  left.normalize();
  right.normalize();

  if (areEqualRelations(left, right))
    return PresburgerRelation({ left });

  return PresburgerRelation({ left, right });
}

PresburgerRelation IntegerRelation::setDifference(const IntegerRelation &lhs, const IntegerRelation &rhs) {
  lhs.space.assertCompatible(rhs.space);
  assert(lhs.space.local == rhs.space.local);

  std::vector<IntegerRelation> pieces;

  for (unsigned r = 0; r < rhs.eqs.rowCount(); ++r) {
    auto eq = extractRow(rhs.eqs, r);

    IntegerRelation ge = lhs;
    ge.addInequality(shiftedLowerBound(eq));
    ge.normalize();
    pieces.push_back(std::move(ge));

    IntegerRelation le = lhs;
    le.addInequality(strictNegation(eq));
    le.normalize();
    pieces.push_back(std::move(le));
  }

  for (unsigned r = 0; r < rhs.ineqs.rowCount(); ++r) {
    IntegerRelation piece = lhs;
    piece.addInequality(strictNegation(extractRow(rhs.ineqs, r)));
    piece.normalize();
    pieces.push_back(std::move(piece));
  }

  return PresburgerRelation(std::move(pieces));
}

PresburgerRelation IntegerRelation::subtract(const IntegerRelation &other) const {
  return setDifference(*this, other);
}

bool PresburgerRelation::isEmpty() const {
  for (const auto &disjunct : disjuncts)
    if (!disjunct.isEmpty())
      return false;
  return true;
}

void PresburgerRelation::normalize() {
  std::vector<IntegerRelation> normalized;
  normalized.reserve(disjuncts.size());

  for (auto disjunct : disjuncts) {
    disjunct.normalize();
    if (disjunct.isEmpty())
      continue;

    bool duplicate = false;
    for (const auto &existing : normalized) {
      if (areEqualRelations(existing, disjunct)) {
        duplicate = true;
        break;
      }
    }

    if (!duplicate)
      normalized.push_back(std::move(disjunct));
  }

  disjuncts = std::move(normalized);
}

void PresburgerRelation::dump(std::ostream &os) const {
  os << "{\n";
  for (unsigned i = 0; i < disjuncts.size(); ++i) {
    os << "  disjunct " << i << ":\n";
    disjuncts[i].dump(os);
  }
  os << "}\n";
}

PresburgerRelation PresburgerRelation::intersection(const PresburgerRelation &lhs, const PresburgerRelation &rhs) {
  std::vector<IntegerRelation> result;
  result.reserve(lhs.disjuncts.size() * rhs.disjuncts.size());

  for (const auto &left : lhs.disjuncts) {
    for (const auto &right : rhs.disjuncts) {
      auto piece = IntegerRelation::intersection(left, right);
      if (!piece.isEmpty())
        result.push_back(std::move(piece));
    }
  }

  PresburgerRelation relation(std::move(result));
  relation.normalize();
  return relation;
}

PresburgerRelation PresburgerRelation::setUnion(const PresburgerRelation &lhs, const PresburgerRelation &rhs) {
  std::vector<IntegerRelation> result;
  result.reserve(lhs.disjuncts.size() + rhs.disjuncts.size());

  result.insert(result.end(), lhs.disjuncts.begin(), lhs.disjuncts.end());
  result.insert(result.end(), rhs.disjuncts.begin(), rhs.disjuncts.end());

  PresburgerRelation relation(std::move(result));
  relation.normalize();
  return relation;
}

PresburgerRelation PresburgerRelation::setDifference(const PresburgerRelation &lhs, const PresburgerRelation &rhs) {
  std::vector<IntegerRelation> result;

  for (const auto &left : lhs.disjuncts) {
    PresburgerRelation running(left);
    for (const auto &right : rhs.disjuncts) {
      std::vector<IntegerRelation> next;
      for (const auto &piece : running.getDisjuncts()) {
        auto diff = IntegerRelation::setDifference(piece, right);
        const auto &diffDisjuncts = diff.getDisjuncts();
        next.insert(next.end(), diffDisjuncts.begin(), diffDisjuncts.end());
      }
      running = PresburgerRelation(std::move(next));
      running.normalize();
      if (running.isEmpty())
        break;
    }

    const auto &survivors = running.getDisjuncts();
    result.insert(result.end(), survivors.begin(), survivors.end());
  }

  PresburgerRelation relation(std::move(result));
  relation.normalize();
  return relation;
}

std::ostream &operator<<(std::ostream &os, const IntegerRelation &relation) {
  relation.dump(os);
  return os;
}

std::ostream &operator<<(std::ostream &os, const PresburgerRelation &relation) {
  relation.dump(os);
  return os;
}

} // namespace pres
