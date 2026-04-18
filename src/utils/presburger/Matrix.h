#ifndef MATRIX_H
#define MATRIX_H

#include <cstring>
#include <cassert>
#include "Fraction.h"

namespace pres {

template<class Int>
class MatrixBase {
  unsigned rows, cols;
  Int *data;
public:
  using value_type = Int;

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

  unsigned rowCount() const { return rows; }
  unsigned colCount() const { return cols; }
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

  void removeRow(unsigned r) {
    assert(r < rows);

    Int *d = new Int[(rows - 1) * cols];

    size_t newr = 0;
    for (size_t i = 0; i < rows; ++i) {
      if (i == r)
        continue;

      for (size_t c = 0; c < cols; c++)
        d[newr * cols + c] = data[i * cols + c];
      
      ++newr;
    }

    delete[] data;
    data = d;
    --rows;
  }

  void removeCol(size_t c) {
    assert(c < cols);

    Int *d = new Int[rows * (cols - 1)];

    for (size_t r = 0; r < rows; ++r) {
      size_t new_c = 0;
      for (size_t i = 0; i < cols; ++i) {
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

  void insertRow(size_t r) {
    assert(r < rows);

    Int *d = new Int[(rows + 1) * cols]();

    for (size_t i = 0; i < rows + 1; ++i) {
      if (i == r) 
        continue;

      size_t src = (i < r) ? i : i - 1;
      for (size_t c = 0; c < cols; ++c)
        d[i * cols + c] = data[src * cols + c];
    }

    delete[] data;
    data = d;
    ++rows;
  }

  void insertCol(size_t c) {
    assert(c < cols);

    Int* d = new Int[rows * (cols + 1)]();

    for (size_t r = 0; r < rows; ++r) {
      for (size_t i = 0; i < cols + 1; ++i) {
        if (i == c)
          continue;

        size_t src = (i < c) ? i : i - 1;
        d[r * (cols + 1) + i] = data[r * cols + src];
      }
    }

    delete[] data;
    data = d;
    ++cols;
  }
};

using Matrix = MatrixBase<BigInt>;
using FracMatrix = MatrixBase<Fraction>;

}

#endif
