#include "../utils/presburger/Fraction.h"
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace pres;

namespace {

struct TestContext {
  int failures = 0;

  void expect(bool condition, const std::string &message) {
    if (!condition) {
      std::cerr << "[FAIL] " << message << "\n";
      ++failures;
    }
  }
} t;

#define expect t.expect

void expectFractionEq(const Fraction &actual, const Fraction &expected,
                      const std::string &message) {
  expect(actual == expected,
         message + " (expected " + static_cast<std::string>(expected.num) +
             "/" + static_cast<std::string>(expected.den) +
             ", got " + static_cast<std::string>(actual.den) +
             "/" + static_cast<std::string>(actual.den) + ")");
}

void testNormalization() {
  expectFractionEq(Fraction(2, 4), Fraction(1, 2),
                   "fraction should reduce by gcd");
  expectFractionEq(Fraction(3, -9), Fraction(-1, 3),
                   "negative denominator should move to numerator");
  expectFractionEq(Fraction(-6, -8), Fraction(3, 4),
                   "double negative should normalize to positive");
  expectFractionEq(Fraction(0, 9), Fraction(0, 1),
                   "zero numerator should normalize denominator to one");
  expectFractionEq(Fraction(7), Fraction(7, 1),
                   "integer constructor should use denominator one");
}

void testUnaryAndArithmetic() {
  Fraction value(2, 3);
  expectFractionEq(+value, Fraction(2, 3), "unary plus should preserve value");
  expectFractionEq(-value, Fraction(-2, 3), "unary minus should negate numerator");

  Fraction sum = Fraction(1, 6);
  sum += Fraction(1, 3);
  expectFractionEq(sum, Fraction(1, 2), "operator+= should add and normalize");

  Fraction diff = Fraction(5, 6);
  diff -= Fraction(1, 2);
  expectFractionEq(diff, Fraction(1, 3), "operator-= should subtract and normalize");

  Fraction prod = Fraction(3, 5);
  prod *= Fraction(10, 9);
  expectFractionEq(prod, Fraction(2, 3), "operator*= should multiply and normalize");

  Fraction quot = Fraction(7, 8);
  quot /= Fraction(14, 3);
  expectFractionEq(quot, Fraction(3, 16), "operator/= should divide and normalize");

  expectFractionEq(Fraction(1, 4) + Fraction(1, 6), Fraction(5, 12),
                   "operator+ should produce correct sum");
  expectFractionEq(Fraction(1, 4) - Fraction(5, 6), Fraction(-7, 12),
                   "operator- should produce correct difference");
  expectFractionEq(Fraction(-3, 7) * Fraction(14, 9), Fraction(-2, 3),
                   "operator* should produce correct product");
  expectFractionEq(Fraction(2, 5) / Fraction(-4, 15), Fraction(-3, 2),
                   "operator/ should produce correct quotient");
}

void testComparisonsAndPrinting() {
  expect(Fraction(1, 2) == Fraction(2, 4),
         "equal fractions should compare equal after normalization");
  expect(Fraction(1, 2) != Fraction(3, 4),
         "different fractions should compare not equal");
  expect(Fraction(-1, 3) < Fraction(1, 3),
         "operator< should compare rational values");
  expect(Fraction(5, 6) > Fraction(4, 5),
         "operator> should compare rational values");
  expect(Fraction(2, 3) <= Fraction(4, 6),
         "operator<= should accept equality");
  expect(Fraction(2, 3) >= Fraction(1, 2),
         "operator>= should accept larger values");

  std::ostringstream whole;
  whole << Fraction(4);
  expect(whole.str() == "4",
         "stream output should omit denominator when it is one");

  std::ostringstream proper;
  proper << Fraction(-3, 5);
  expect(proper.str() == "-3/5",
         "stream output should print normalized numerator and denominator");
}

} // namespace

namespace test {

void runFractionTests() {
  const std::vector<std::pair<std::string, std::function<void()>>> tests = {
    { "Normalization", testNormalization },
    { "UnaryAndArithmetic", testUnaryAndArithmetic },
    { "ComparisonsAndPrinting", testComparisonsAndPrinting },
  };

  for (const auto &[name, fn] : tests) {
    int before = t.failures;
    fn();
    if (t.failures == before)
      std::cerr << "[PASS] " << name << "\n";
  }

  if (t.failures != 0) {
    std::cerr << t.failures << " test(s) failed\n";
    return;
  }

  std::cerr << "All Fraction unit tests passed\n";
}

}
