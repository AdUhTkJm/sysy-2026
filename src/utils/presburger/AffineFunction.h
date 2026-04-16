#ifndef AFFINE_H
#define AFFINE_H

#include <vector>
#include <cassert>
#include <iostream>

namespace pres {

template<class Int = int>
class AffineFunctionBase {
  std::vector<Int> data;

public:
  using value_type = Int;

  AffineFunctionBase(const std::vector<Int> &coefficients, Int constant) {
    data = coefficients;
    data.push_back(constant);
  }

  AffineFunctionBase(const std::vector<Int> &fullData): data(fullData) {
    assert(!fullData.empty());
  }

  unsigned dimension() const { return data.size() - 1; }

  Int coeff(unsigned i) const {
    assert(i < data.size());
    return data[i];
  }

  Int constant() const { return data.back(); }
  Int operator[](unsigned i) const { return data[i]; }
  Int &operator[](unsigned i) { return data[i]; }

  AffineFunctionBase operator+(const AffineFunctionBase& other) const {
    assert(data.size() == other.data.size());

    std::vector<Int> result(data.size());
    for (size_t i = 0; i < data.size(); i++)
      result[i] = data[i] + other.data[i];

    return result;
  }

  AffineFunctionBase operator-(const AffineFunctionBase& other) const {
    assert(data.size() == other.data.size());

    std::vector<Int> result(data.size());
    for (size_t i = 0; i < data.size(); i++)
      result[i] = data[i] -other.data[i];

    return result;
  }

  void dump(std::ostream &os = std::cerr) const {
    os << "[";
    for (size_t i = 0; i < data.size() - 1; i++)
      os << data[i] << ", ";
    
    os << constant() << "]";
  }
};

template<class T>
std::ostream &operator<<(std::ostream &os, const AffineFunctionBase<T> &base) {
  base.dump(os);
  return os;
}

using AffineFunction = AffineFunctionBase<>;
using Int = AffineFunction::value_type;

struct Domain {
  struct DomainGenerator {
    unsigned dim;

    AffineFunction operator[](unsigned i) {
      assert(i < dim + 1);
      std::vector<Int> ret(dim + 1);
      ret[i] = 1;
      return ret;
    }
  };
  DomainGenerator operator()(unsigned dim) {
    return DomainGenerator { dim };
  };
} extern domain;

struct Constant { 
  AffineFunction operator()(unsigned dim, unsigned value) {
    std::vector<Int> ret(dim + 1);
    ret.back() = value;
    return ret;
  };
} extern constant;

}

#endif
