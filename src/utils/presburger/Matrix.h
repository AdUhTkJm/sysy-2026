#ifndef MATRIX_H
#define MATRIX_H

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#include "Fraction.h"

namespace pres {

template<class Int>
class MatrixBase {
  unsigned rows, cols;
  Int *data;
public:
  using value_type = Int;
  using Row = std::vector<Int>;

  MatrixBase(unsigned r = 0, unsigned c = 0): rows(r), cols(c), data(new Int[r * c]()) {}
  ~MatrixBase() { delete[] data; }

  MatrixBase(const MatrixBase &other): rows(other.rows), cols(other.cols), data(new Int[rows * cols]()) {
    for (unsigned i = 0; i < rows * cols; i++)
      data[i] = other.data[i];
  }

  MatrixBase &operator=(const MatrixBase &other) {
    if (this == &other)
      return *this;

    delete[] data;
    rows = other.rows;
    cols = other.cols;
    unsigned size = rows * cols;

    data = new Int[size];
    for (unsigned i = 0; i < rows * cols; i++)
      data[i] = other.data[i];
    return *this;
  }

  Int &operator()(unsigned r, unsigned c) {
    assert(r < rows && c < cols);
    return data[r * cols + c];
  }

  Int operator()(unsigned r, unsigned c) const {
    assert(r < rows && c < cols);
    return data[r * cols + c];
  }

  Int &at(unsigned r, unsigned c) { return (*this)(r, c); }
  Int at(unsigned r, unsigned c) const { return (*this)(r, c); }

  unsigned getNumRows() const { return rows; }
  unsigned getNumCols() const { return cols; }
  unsigned size() const { return rows * cols; }

  void resize(unsigned nr, unsigned nc) {
    Int* new_data = new Int[nr * nc]();
    unsigned mr = (rows < nr) ? rows : nr;
    unsigned mc = (cols < nc) ? cols : nc;

    for (unsigned r = 0; r < mr; r++) {
      for (unsigned c = 0; c < mc; c++)
        new_data[r * nc + c] = data[r * cols + c];
    }

    delete[] data;
    data = new_data;
    rows = nr;
    cols = nc;
  }

  void appendRow() { resize(rows + 1, cols); }
  void appendCol() { resize(rows, cols + 1); }
  void appendRow(const Row &r) {
    assert(r.size() == cols);
    appendRow();
    for (unsigned i = 0; i < cols; i++)
      at(rows - 1, i) = r[i];
  }

  void removeRow(unsigned r) {
    assert(r < rows);

    Int *d = new Int[(rows - 1) * cols];

    unsigned newr = 0;
    for (unsigned i = 0; i < rows; ++i) {
      if (i == r)
        continue;

      for (unsigned c = 0; c < cols; c++)
        d[newr * cols + c] = data[i * cols + c];
      
      ++newr;
    }

    delete[] data;
    data = d;
    --rows;
  }

  void removeCol(unsigned c) {
    assert(c < cols);

    Int *d = new Int[rows * (cols - 1)];

    for (unsigned r = 0; r < rows; ++r) {
      unsigned new_c = 0;
      for (unsigned i = 0; i < cols; ++i) {
        if (i == c)
          continue;

        d[r * (cols - 1) + new_c] = data[r * cols + i];
        ++new_c;
      }
    }

    delete[] data;
    data = d;
    --cols;
  }

  void insertRow(unsigned r) {
    assert(r < rows);

    Int *d = new Int[(rows + 1) * cols]();

    for (unsigned i = 0; i < rows + 1; ++i) {
      if (i == r) 
        continue;

      unsigned src = (i < r) ? i : i - 1;
      for (unsigned c = 0; c < cols; ++c)
        d[i * cols + c] = data[src * cols + c];
    }

    delete[] data;
    data = d;
    ++rows;
  }

  void insertCol(unsigned c) {
    assert(c < cols);

    Int* d = new Int[rows * (cols + 1)]();

    for (unsigned r = 0; r < rows; ++r) {
      for (unsigned i = 0; i < cols + 1; ++i) {
        if (i == c)
          continue;

        unsigned src = (i < c) ? i : i - 1;
        d[r * (cols + 1) + i] = data[r * cols + src];
      }
    }

    delete[] data;
    data = d;
    ++cols;
  }

  Row row(unsigned r) const {
    Row row;
    row.reserve(cols);
    for (unsigned i = 0; i < cols; i++)
      row.push_back(at(r, i));
    return row;
  }

  Int normalizeRow(unsigned r, unsigned limit = -1u) {
    Int gcd = abs(at(r, 0));
    for (unsigned i = 1; i < std::min(limit, cols); i++)
      gcd = pres::gcd(gcd, at(r, i));
    if (gcd == 0)
      return 0;
    
    for (unsigned i = 0; i < std::min(limit, cols); i++)
      at(r, i) /= gcd;
    return gcd;
  }

  void dump(std::ostream &os = std::cerr, int indent = 2) const {
    size_t maxWidth = 6;
    std::vector<std::string> elements(rows * cols);
    for (unsigned r = 0; r < rows; ++r) {
      for (unsigned c = 0; c < cols; ++c) {
        std::ostringstream ss;
        ss << at(r, c);
        std::string str = ss.str();
        elements[r * cols + c] = str;

        bool negative = !str.empty() && str[0] == '-';
        size_t valueWidth = negative ? str.size() - 1 : str.size();
        maxWidth = std::max(maxWidth, valueWidth);
      }
    }

    std::ios::fmtflags flags = os.flags();
    char fill = os.fill();

    for (unsigned r = 0; r < rows; ++r) {
      if (indent > 0)
        os << std::string(indent, ' ');
      for (unsigned c = 0; c < cols; ++c) {
        const std::string &str = elements[r * cols + c];
        bool negative = !str.empty() && str[0] == '-';
        const std::string number = negative ? str.substr(1) : str;

        os << (negative ? '-' : ' ');
        os << std::left << std::setw(maxWidth) << std::setfill(' ') << number;
      }
      os << '\n';
    }

    os.flags(flags);
    os.fill(fill);
  }
};

using Matrix = MatrixBase<BigInt>;
using FracMatrix = MatrixBase<Fraction>;

}

#endif
