#include "../../../3rdparty/BigInt.hpp"
#include <cassert>
#include <ostream>

namespace pres {

template<class Int>
class FractionBase {
  Int nom, den;
public:
  void normalize() {
    assert(den != 0);

    if (den < 0) {
      nom = -nom;
      den = -den;
    }

    if (nom == 0) {
      den = 1;
      return;
    }

    auto g = gcd(abs(nom), den);
    if (g != 1) {
      nom /= g;
      den /= g;
    }
  }

  FractionBase(): nom(0), den(1) {}
  FractionBase(const Int &value): nom(value), den(1) {}
  FractionBase(const Int &numerator, const Int &denominator): nom(numerator), den(denominator) {
    normalize();
  }

  const Int &numerator() const { return nom; }
  const Int &denominator() const { return den; }

  FractionBase operator+() const { return *this; }
  FractionBase operator-() const { return FractionBase(-nom, den); }

  FractionBase &operator+=(const FractionBase &other) {
    nom = nom * other.den + other.nom * den;
    den *= other.den;
    normalize();
    return *this;
  }

  FractionBase &operator-=(const FractionBase &other) {
    nom = nom * other.den - other.nom * den;
    den *= other.den;
    normalize();
    return *this;
  }

  FractionBase &operator*=(const FractionBase &other) {
    nom *= other.nom;
    den *= other.den;
    normalize();
    return *this;
  }

  FractionBase &operator/=(const FractionBase &other) {
    assert(other.nom != 0);
    nom *= other.den;
    den *= other.nom;
    normalize();
    return *this;
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
    return lhs.nom == rhs.nom && lhs.den == rhs.den;
  }

  friend bool operator!=(const FractionBase &lhs, const FractionBase &rhs) {
    return !(lhs == rhs);
  }

  friend bool operator<(const FractionBase &lhs, const FractionBase &rhs) {
    return lhs.nom * rhs.den < rhs.nom * lhs.den;
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
    os << fraction.nom;
    if (fraction.den != 1)
      os << "/" << fraction.den;
    return os;
  }

};

using Fraction = FractionBase<BigInt>;

}
