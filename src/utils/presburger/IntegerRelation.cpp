#include "IntegerRelation.h"
#include <cassert>
#include <ostream>
#include <utility>

namespace pres {

namespace {

using Int = IntegerRelation::Int;
using Row = std::vector<Int>;

void assertColumnCount(const Matrix &matrix, unsigned cols) {
  assert(matrix.colCount() == cols);
}

Row shiftedLB(const Row &row) {
  auto result = row;
  --result.back();
  return result;
}

Row negate(const Row &row) {
  auto result = row;
  for (Int &value : result)
    value = -value;
  --result.back();
  return result;
}

Int floorDiv(Int num, Int den) {
  assert(den > 0);
  if (num >= 0)
    return num / den;
  return -(((-num) + den - 1) / den);
}

} // namespace

IntegerRelation::IntegerRelation(Space s, const std::vector<Row> &eqs, const std::vector<Row> &ineqs):
  eqs(0, s.getNumCols()), ineqs(0, s.getNumCols()), space(s) {
  for (const auto &row : eqs)
    addEquality(row);
  for (const auto &row : ineqs)
    addInequality(row);
  simplify();
}

IntegerRelation::IntegerRelation(Space space, const Matrix &equalities, const Matrix &inequalities):
  eqs(equalities), ineqs(inequalities), space(space) {
  assertColumnCount(eqs, getNumCols());
  assertColumnCount(ineqs, getNumCols());
}

IntegerRelation::IntegerRelation(Space space, const IntegerRelation::Row &point): pres::IntegerRelation(space) {
  for (unsigned i = 0; i < point.size(); ++i) {
    IntegerRelation::Row row(space.dimension() + 1, 0);
    row[i] = 1;
    row.back() = -point[i];
    addEquality(row);
  }
  simplify();
}

bool IntegerRelation::isObviouslyEmpty() const {
  for (unsigned r = 0; r < eqs.rowCount(); ++r) {
    bool zero = true;
    for (unsigned c = 0; c + 1 < eqs.colCount(); ++c) {
      if (eqs(r, c) != 0) {
        zero = false;
        break;
      }
    }
    if (zero && eqs(r, eqs.colCount() - 1) != 0)
      return true;
  }

  for (unsigned r = 0; r < ineqs.rowCount(); ++r) {
    bool zero = true;
    for (unsigned c = 0; c + 1 < ineqs.colCount(); ++c) {
      if (ineqs(r, c) != 0) {
        zero = false;
        break;
      }
    }
    if (zero && ineqs(r, ineqs.colCount() - 1) < 0)
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
  normalized.simplify();
  if (normalized.isObviouslyEmpty())
    return true;

  return !findSamplePoint();
}

std::optional<Row> IntegerRelation::findSamplePoint() const {
  return std::nullopt; // TODO: stub
}

void IntegerRelation::simplify() {
  gcdTightenInequalities();
}

void IntegerRelation::gcdTightenInequalities() {
  unsigned numCols = getNumCols();
  for (unsigned i = 0, e = getNumInequalities(); i < e; ++i) {
    Int gcd = ineqs.normalizeRow(i, numCols - 1);
    if (gcd > 1)
      ineqs(i, numCols - 1) = floorDiv(ineqs(i, numCols - 1), gcd);
  }
}

void IntegerRelation::dump(std::ostream &os) const {
  os << "Domain: " << space.domain << ", Range: " << space.range << ", Symbol: "
     << space.symbol << ", Local: " << space.local << "\n\n";

  os << "Eqs:\n";
  eqs.dump();
  os << "Ineqs:\n";
  ineqs.dump();
}

void pivot(FracMatrix &matrix, unsigned row, unsigned col) {
  Fraction pivot_val = matrix(row, col);
  
  // Normalize the row.
  for (unsigned j = 0; j < matrix.colCount(); ++j)
    matrix(row, j) /= pivot_val;

  for (unsigned i = 0; i < matrix.rowCount(); ++i) {
    if (i == row)
      continue;
    // To make sure the `col`'th column satisfy:
    //    - `at(row, col) == 1`;
    //    - `at(row, j) == 0` for any other j,
    //
    // we need to perform:
    //    matrix[i] -= matrix(i, col) * matrix[row]
    for (unsigned j = 0; j < matrix.colCount(); ++j)
      matrix(i, j) -= matrix(i, col) * matrix(row, j);
  }
}

// The entering basis is the one with the smallest negative coefficient in the target row (i.e. the last row).
int findEntering(const FracMatrix &tableau) {
  int target = tableau.rowCount() - 1;
  int best = -1;
  Fraction min(0);

  for (unsigned j = 0; j < tableau.colCount() - 1; ++j) {
    if (tableau(target, j) < min) {
      min = tableau(target, j);
      best = j;
    }
  }
  return best;
}

int findLeaving(const FracMatrix &tableau, int p_col) {
  int best = -1;
  Fraction min(-1); 

  for (unsigned i = 0; i < tableau.rowCount() - 1; ++i) {
    if (tableau(i, p_col) <= 0)
      continue;
    
    Fraction ratio = tableau(i, tableau.colCount() - 1) / tableau(i, p_col);
    if (best == -1 || ratio < min) {
      min = ratio;
      best = i;
    }
  }
  return best;
}

void simplex(FracMatrix &tableau, std::vector<unsigned> &basis) {
  while (true) {
    int col = findEntering(tableau);
    if (col == -1)
      break;

    int row = findLeaving(tableau, col);
    if (row == -1)
      assert(false && "unbounded");

    pivot(tableau, row, col);
    basis[row] = col;
  }
}

// We must maintain that, in the target row, all coefficients of the basis variables are zero.
void initialBaseElim(FracMatrix &tableau, std::vector<unsigned> &basis) {
  unsigned target = tableau.rowCount() - 1;
  for (unsigned i = 0; i < basis.size(); i++) {
    if (Fraction factor = tableau(target, basis[i]); factor != 0) {
      for (unsigned j = 0; j < tableau.colCount(); j++)
        tableau(target, j) -= factor * tableau(i, j);
    }
  }
}

Fraction IntegerRelation::getRationalMin(const Row &target) const {  
  unsigned dim = dimension();
  unsigned rows = getNumEqualities() + getNumInequalities();
  unsigned artificial = getNumEqualities(), slack = getNumInequalities();
  for (unsigned i = 0; i < eqs.rowCount(); i++) {
    // For Ax == -b, we introduce am artificial variable `a` such that
    //   Ax + a == -b (and `a` must be zero; we're enforcing that with two-phase method).
    // We will first assign `a` in the basis. All non-basis variables are set to zero,
    // which means `a` will become -b. As a >= 0, we must make sure b <= 0.
    if (eqs(i, dim) <= 0)
      continue;

    for (unsigned j = 0; j <= dim; j++)
      eqs(i, j) *= -1;
  }
  std::vector<char> sign(ineqs.rowCount());
  for (unsigned i = 0; i < ineqs.rowCount(); i++) {
    // For Ax >= -b, we first introduce a slack variable `s` such that
    //   Ax - s == -b
    // Here s >= 0. If b <= 0, then this poses no problem and we can directly add an artifical variable.
    //
    // If b > 0, however, we must first rewrite it as -Ax <= b.
    // In this case what we have is -Ax + s == b, and s is simply a slack variable,
    // and is not artificial. Yet it is still put in the basis.
    if (eqs(i, dim) <= 0) {
      sign[i] = '+';
      artificial++;
      continue;
    }

    for (unsigned j = 0; j <= dim; j++)
      eqs(i, j) *= -1;
    sign[i] = '-';
  }

  unsigned cols = dim * 2 + slack + artificial + 1 /* Constant */;
  FracMatrix tableau(rows, cols);
  std::vector<unsigned> basis;
  std::vector<Fraction> auxTarget(cols);
  basis.reserve(rows);

  // Fill it in.
  for (unsigned i = 0; i < eqs.rowCount(); i++) {
    // Split variable `j` into `s_(2j) - s_(2j+1)`.
    for (unsigned j = 0; j < dim; j++) {
      tableau(i, j * 2) = eqs(i, j);
      tableau(i, j * 2 + 1) = -eqs(i, j);
    }
    // Artificial variable, must be zero.
    unsigned index = dim * 2 + slack + i;
    tableau(i, index) = 1;
    basis.push_back(index);
    auxTarget[index] = 1;
    // Recodr constant.
    tableau(i, cols - 1) = -eqs(i, dim);
  }

  unsigned index = dim * 2 + slack + eqs.rowCount();
  for (unsigned i = 0; i < ineqs.rowCount(); i++) {
    for (unsigned j = 0; j < dim; j++) {
      tableau(i, j * 2) = ineqs(i, j);
      tableau(i, j * 2 + 1) = -ineqs(i, j);
    }
    tableau(i, cols - 1) = -ineqs(i, dim);

    if (sign[i] == '+') {
      // Artificial variable.
      tableau(i, index) = 1;
      basis.push_back(index);
      auxTarget[index] = 1;
      index++;
      // Slack variable.
      tableau(i, dim * 2 + i) = -1;
    } else {
      // Slack variable only.
      tableau(i, dim * 2 + i) = 1;
      basis.push_back(dim * 2 + i);
    }
  }

  // Phase I. Remove all artificial variables from the basis.
  tableau.appendRow(auxTarget);
  simplex(tableau, basis);

  // Remove all artificial variables.
  // First make sure there's no artificial variables after that.
  for (unsigned i = 0; i < basis.size(); i++) {
    if (dim * 2 + slack > basis[i] || basis[i] >= dim * 2 + slack + artificial)
      continue;
    
    // An artificial variable still sits in the basis. We must swap it out.
    bool replaced = false;
    for (unsigned j = 0; j < cols; ++j) {
      if (abs(tableau(i, j)) != 0) {
        pivot(tableau, i, j);
        basis[i] = j;
        replaced = true;
        break;
      }
    }
    // The entire row is zero. We can directly remove it.
    if (!replaced) {
      tableau.removeRow(basis[i]);
      basis.erase(basis.begin() + i);
      i--;
    }
  }

  // Copy them into a new tableau.
  FracMatrix t(rows = tableau.rowCount(), cols = dim * 2 + slack + 1);
  for (unsigned i = 0; i < rows; i++) {
    for (unsigned j = 0; j < t.colCount() - 1; j++)
      t(i, j) = tableau(i, j);
    t(i, t.colCount() - 1) = tableau(i, cols - 1);
  }  
  tableau = std::move(t);

  // Add the target to the tableau.
  std::vector<Fraction>(cols).swap(auxTarget);
  for (unsigned i = 0; i < target.size(); i++) {
    auxTarget[2 * i] = target[i];
    auxTarget[2 * i + 1] = -target[i];
  }
  tableau.appendRow(auxTarget);

  initialBaseElim(tableau, basis);

  // Do the simplex and read off the result.
  simplex(tableau, basis);
  return tableau(rows - 1, cols - 1);
}

IntegerRelation IntegerRelation::intersection(const IntegerRelation &lhs, const IntegerRelation &rhs) {
  // Simply concatenate the (in-)equalities matrices.
  lhs.space.assertCompatible(rhs.space);
  assert(lhs.space.local == rhs.space.local);

  IntegerRelation result(lhs.space);
  result.eqs.resize(lhs.eqs.rowCount() + rhs.eqs.rowCount(), lhs.getNumCols());
  result.ineqs.resize(lhs.ineqs.rowCount() + rhs.ineqs.rowCount(), lhs.getNumCols());

  unsigned out = 0;
  for (unsigned r = 0, out = 0; r < lhs.eqs.rowCount(); ++r, ++out)
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

  return result;
}

PresburgerRelation IntegerRelation::setUnion(const IntegerRelation &lhs, const IntegerRelation &rhs) {
  // Perform a basic conjunction.
  lhs.space.assertCompatible(rhs.space);
  assert(lhs.space.local == rhs.space.local);

  return PresburgerRelation({ lhs, rhs });
}

PresburgerRelation IntegerRelation::setDifference(const IntegerRelation &lhs, const IntegerRelation &rhs) {
  lhs.space.assertCompatible(rhs.space);
  assert(lhs.space.local == rhs.space.local);

  std::vector<IntegerRelation> pieces;

  for (unsigned r = 0; r < rhs.eqs.rowCount(); ++r) {
    auto eq = rhs.eqs.row(r);

    IntegerRelation ge = lhs;
    ge.addInequality(shiftedLB(eq));
    pieces.push_back(std::move(ge));

    IntegerRelation le = lhs;
    le.addInequality(negate(eq));
    pieces.push_back(std::move(le));
  }

  for (unsigned r = 0; r < rhs.ineqs.rowCount(); ++r) {
    IntegerRelation piece = lhs;
    piece.addInequality(negate(rhs.ineqs.row(r)));
    pieces.push_back(std::move(piece));
  }

  return PresburgerRelation(std::move(pieces));
}

bool PresburgerRelation::isEmpty() const {
  for (const auto &disjunct : disjuncts) {
    if (!disjunct.isEmpty())
      return false;
  }
  return true;
}

void PresburgerRelation::simplify() {
  std::vector<IntegerRelation> normalized;
  normalized.reserve(disjuncts.size());

  for (auto disjunct : disjuncts) {
    disjunct.simplify();
    if (disjunct.isEmpty())
      continue;

    normalized.push_back(std::move(disjunct));
  }

  disjuncts = std::move(normalized);
}

void PresburgerRelation::dump(std::ostream &os) const {
  for (unsigned i = 0; i < disjuncts.size(); ++i) {
    os << "Disjunct " << i << ":\n";
    disjuncts[i].dump(os);
  }
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
  relation.simplify();
  return relation;
}

PresburgerRelation PresburgerRelation::setUnion(const PresburgerRelation &lhs, const PresburgerRelation &rhs) {
  std::vector<IntegerRelation> result;
  result.reserve(lhs.disjuncts.size() + rhs.disjuncts.size());

  result.insert(result.end(), lhs.disjuncts.begin(), lhs.disjuncts.end());
  result.insert(result.end(), rhs.disjuncts.begin(), rhs.disjuncts.end());

  PresburgerRelation relation(std::move(result));
  relation.simplify();
  return relation;
}

PresburgerRelation PresburgerRelation::setDifference(const PresburgerRelation &lhs, const PresburgerRelation &rhs) {
  std::vector<IntegerRelation> result;

  for (const auto &left : lhs.disjuncts) {
    PresburgerRelation running(left);
    for (const auto &right : rhs.disjuncts) {
      std::vector<IntegerRelation> next;
      for (const auto &piece : running.disjuncts) {
        auto diff = IntegerRelation::setDifference(piece, right);
        const auto &diffDisjuncts = diff.disjuncts;
        next.insert(next.end(), diffDisjuncts.begin(), diffDisjuncts.end());
      }
      running = PresburgerRelation(std::move(next));
      running.simplify();
      if (running.isEmpty())
        break;
    }

    const auto &survivors = running.disjuncts;
    result.insert(result.end(), survivors.begin(), survivors.end());
  }

  PresburgerRelation relation(std::move(result));
  relation.simplify();
  return relation;
}

bool PresburgerRelation::containsPoint(const Row &point) const {
  for (const auto &disjunct : disjuncts) {
    if (disjunct.containsPoint(point))
      return false;
  }
  return true;
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
