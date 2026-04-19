#include "../../../3rdparty/BigInt.hpp"
#include <cassert>
#include <ostream>

namespace pres {

template<class Int>
class FractionBase {
public:
  Int num, den;
  void simplify() {
    assert(den != 0);

    if (den < 0) {
      num = -num;
      den = -den;
    }

    if (num == 0) {
      den = 1;
      return;
    }

    auto g = gcd(abs(num), den);
    if (g != 1) {
      num /= g;
      den /= g;
    }
  }

  bool isInteger() const { return den == 1; }

  FractionBase(): num(0), den(1) {}
  FractionBase(int value): FractionBase(Int(value)) {}
  FractionBase(const Int &value): num(value), den(1) {}
  FractionBase(const Int &numerator, const Int &denominator): num(numerator), den(denominator) {
    simplify();
  }

  FractionBase operator+() const { return *this; }
  FractionBase operator-() const { return FractionBase(-num, den); }

  FractionBase &operator+=(const FractionBase &other) {
    num = num * other.den + other.num * den;
    den *= other.den;
    simplify();
    return *this;
  }

  FractionBase &operator-=(const FractionBase &other) {
    num = num * other.den - other.num * den;
    den *= other.den;
    simplify();
    return *this;
  }

  FractionBase &operator*=(const FractionBase &other) {
    num *= other.num;
    den *= other.den;
    simplify();
    return *this;
  }

  FractionBase &operator/=(const FractionBase &other) {
    assert(other.num != 0);
    num *= other.den;
    den *= other.num;
    simplify();
    return *this;
  }

  explicit operator Int() const {
    assert(isInteger());
    return num;
  }

  friend FractionBase operator+(FractionBase lhs, const FractionBase &rhs) {
    lhs += rhs;
    return lhs;
  }

  friend FractionBase operator-(FractionBase lhs, const FractionBase &rhs) {
    lhs -= rhs;
    return lhs;
  }

  friend FractionBase operator*(FractionBase lhs, const FractionBase &rhs) {
    lhs *= rhs;
    return lhs;
  }

  friend FractionBase operator/(FractionBase lhs, const FractionBase &rhs) {
    lhs /= rhs;
    return lhs;
  }

  friend bool operator==(const FractionBase &lhs, const FractionBase &rhs) {
    return lhs.num == rhs.num && lhs.den == rhs.den;
  }

  friend bool operator!=(const FractionBase &lhs, const FractionBase &rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<(const FractionBase &lhs, const FractionBase &rhs) {
    return lhs.num * rhs.den < rhs.num * lhs.den;
  }

  friend bool operator>(const FractionBase &lhs, const FractionBase &rhs) {
    return rhs < lhs;
  }

  friend bool operator<=(const FractionBase &lhs, const FractionBase &rhs) {
    return !(rhs < lhs);
  }

  friend bool operator>=(const FractionBase &lhs, const FractionBase &rhs) {
    return !(lhs < rhs);
  }

  friend std::ostream &operator<<(std::ostream &os, const FractionBase &fraction) {
    os << fraction.num;
    if (fraction.den != 1)
      os << "/" << fraction.den;
    return os;
  }
};

template<class Int>
FractionBase<Int> abs(const FractionBase<Int> &x) {
  return x < 0 ? -x : x;
}

using Fraction = FractionBase<BigInt>;

}
