/*
  BigInt Library for C++

  MIT License
  
  Copyright (c) 2024 Samuel Herts
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

// Modified myself to tailor my own needs.

#ifndef BIGINT_H_
#define BIGINT_H_

#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <cmath>
#include <cassert>
#include <iostream>
#include <functional>
#include <algorithm>
#include <random>
#include <iomanip>

namespace pres {

class BigInt {
public:
  //           LLONG_MAX = 9'223'372'036'854'775'807
  static constexpr auto MAX_SIZE = 1000000000LL;

  BigInt() : vec({0}) {}

  BigInt(const std::string &s) {
    assert(!isBigint(s)) ;
    if (s[0] == '-') {
      *this = BigInt(s.substr(1));
      if (*this != 0)
        neg = true;
    }
    else if (s[0] == '0' && s[1] == 'x') {
      neg = false;
      vec = hex2vec(s);
    }
    else {
      neg = false;
      vec = str2vec(s);
    }
  }

  BigInt(const char c) {
    const int temp = static_cast<unsigned char>(c);
    assert(isdigit(temp));
    *this = BigInt(digitize(c));
  }

  BigInt(const char* n) : BigInt(std::string(n)) {}

  BigInt(const int n) :       BigInt(static_cast<long long>(n)) {}
  BigInt(const unsigned int n) :  BigInt(static_cast<long long>(n)) {}
  BigInt(const long n) :      BigInt(static_cast<long long>(n)) {}
  BigInt(const unsigned long n) : BigInt(static_cast<long long>(n)) {}
  BigInt(const double n) :    BigInt(static_cast<long long>(n)) {}

  BigInt(const long long n): neg(n < 0) {
    if (n == 0) {
      neg = false;
      vec.push_back(0);
      return;
    }
    unsigned long long val = n;
    if (neg) {
      val = 0ULL - val;
    }
    vec.reserve(2);

    while (val > 0) {
      vec.emplace_back(val % MAX_SIZE);
      val /= MAX_SIZE;
    }
    std::reverse(vec.begin(), vec.end());
  }

  BigInt(unsigned long long n) {
    if (n >= MAX_SIZE) {
      vec.emplace_back(n / MAX_SIZE);
      vec.emplace_back(n % MAX_SIZE);
    }
    else {
      vec.emplace_back(n);
    }
  }

  BigInt(const BigInt &n) = default;

  /* If initializing from a vector that should be negative, the negative value must be set afterward.
    * bigint alpha(std::vector(...));
    * -alpha;
    */
  BigInt(std::vector<long long> n) : vec(std::move(n)) {}

  BigInt &operator=(const BigInt &other) {
    if (this == &other)
      return *this;
    this->neg = other.neg;
    this->vec = other.vec;

    return *this;
  }

  explicit operator int() const {
    return static_cast<int>(vec.back());
  }

  explicit operator long long() const {
    return vec.back();
  }

  explicit operator std::string() const {
    return (this->neg ? "-" : "") + vec2str(this->vec);
  }

  friend std::ostream &operator<<(std::ostream &stream, const BigInt &n) {
    stream << std::string(n);
    return stream;
  }

  BigInt operator+=(const BigInt &rhs) {
    if (*this == 0 && rhs == 0) return *this;
    if (*this == 0) {
      *this = rhs;
      return *this;
    }
    if (rhs != 0) {
      *this = add(*this, rhs);
    }
    return *this;
  }

  BigInt operator+(const BigInt &rhs) const {
    BigInt result = *this;
    result += rhs;
    return result;
  }

  BigInt operator-=(const BigInt &rhs) {
    if (rhs == 0) { return *this; }
    if (*this == rhs) {
      *this = 0;
      return *this;
    }
    *this = subtract(*this, rhs);
    return *this;
  }

  BigInt operator-(const BigInt &rhs) const {
    BigInt result = *this;
    result -= rhs;
    return result;
  }

  BigInt operator*=(const BigInt &rhs) {
    *this = multiply(*this, rhs);
    return *this;
  }

  BigInt operator*(const BigInt &rhs) const {
    BigInt result = *this;
    result *= rhs;
    return result;
  }

  BigInt &operator/=(const BigInt &rhs) {
    *this = divide(*this, rhs);
    return *this;
  }

  BigInt operator/(const BigInt &rhs) const {
    BigInt result = *this;
    result /= rhs;
    return result;
  }

  BigInt operator%=(const BigInt &rhs) {
    *this = mod(*this, rhs);
    return *this;
  }

  BigInt operator%(const BigInt &rhs) const {
    BigInt result = *this;
    result %= rhs;
    return result;
  }

  BigInt operator++() {
    *this += 1;
    return *this;
  }

  BigInt operator++(int) {
    BigInt tmp(*this);
    operator++();
    return tmp;
  }

  BigInt operator--() {
    *this -= 1;
    return *this;
  }

  BigInt operator--(int) {
    BigInt tmp(*this);
    operator--();
    return tmp;
  }

  BigInt operator-() const  &{
    BigInt temp = *this;
    if (temp == 0) return temp;
    temp.neg = !this->neg;
    return temp;
  }

  BigInt operator-() && {
    if (*this == 0) return *this;
    this->neg = !this->neg;
    return *this;
  }

  friend bool operator==(const BigInt &l, const BigInt &r) {
    if (l.neg != r.neg) {
      return false;
    }
    return l.vec == r.vec;
  }

  friend bool operator!=(const BigInt &l, const BigInt &r) {
    return !(l == r);
  }

  friend bool operator<(const BigInt &lhs, const BigInt &rhs) {
    return lessThan(lhs, rhs);
  }

  friend bool operator>(const BigInt &l, const BigInt &r) {
    return r < l;
  }

  friend bool operator<=(const BigInt &l, const BigInt &r) {
    return r >= l;
  }

  friend bool operator>=(const BigInt &l, const BigInt &r) {
    return !(l < r);
  }

  explicit operator bool() const {
    return !(vec.size() == 1 && vec.front() == 0);
  }

  friend std::hash<BigInt>;

  static BigInt pow(const BigInt &base, const BigInt &exponent) {
    if (exponent == 0) return 1;
    if (exponent == 1) return base;

    const BigInt tmp = pow(base, exponent / 2);
    if (exponent % 2 == 0) { return tmp * tmp; }
    return base * tmp * tmp;
  }

  static BigInt maximum(const BigInt &lhs, const BigInt &rhs) {
    return lhs > rhs ? lhs : rhs;
  }

  static BigInt minimum(const BigInt &lhs, const BigInt &rhs) {
    return lhs > rhs ? rhs : lhs;
  }

  static BigInt abs(const BigInt &s) {
    if (!negative(s)) return s;

    BigInt temp = s;
    temp.neg = false;

    return temp;
  }

  static BigInt abs(BigInt&& s) {
    s.neg = false;
    return s;
  }

  static BigInt sqrt(const BigInt&);

  static BigInt log2(const BigInt&);

  static BigInt log10(const BigInt&);

  static BigInt logwithbase(const BigInt&, const BigInt&);

  static BigInt antilog2(const BigInt&);

  static BigInt antilog10(const BigInt&);

  static void swap(BigInt&, BigInt&);

  static BigInt gcd(const BigInt&, const BigInt&);

  static BigInt lcm(const BigInt &lhs, const BigInt &rhs) {
    return (lhs * rhs) / gcd(lhs, rhs);
  }

  static BigInt factorial(const BigInt&);

  static bool even(const BigInt &input) {
    return !(input.vec.back()  &1);
  }

  static bool negative(const BigInt &input) {
    return input.neg;
  }

  static bool prime(const BigInt&);

  static BigInt sumOfDigits(const BigInt &input) {
    BigInt sum;
    for (auto base : input.vec) {
      for (sum = 0; base > 0; sum += base % 10, base /= 10);
    }
    return sum;
  }

  /**
    * @brief Generates a random positive bigint of a specified length.
    *
    * This method ensures the resulting bigint is valid by using a random device
    * and engine for non-deterministic seeding.
    *
    * @param length The number of digits the generated bigint should have.
    * @return A bigint object representing the randomly generated number.
    */
  static BigInt random(size_t length);

private:
  bool neg{false};
  std::vector<long long> vec;

  // Function Definitions for Internal Uses
  static BigInt trim(BigInt input) {
    while (input.vec.size() > 1 && input.vec.front() == 0) {
      input.vec.erase(input.vec.begin());
    }
    if (input.vec.empty()) {
      input.vec.push_back(0);
      input.neg = false;
    }
    return input;
  }

  static std::vector<long long> str2vec(std::string input);
  static std::vector<long long> hex2vec(std::string input);

  static std::string vec2str(const std::vector<long long> &input);

  static BigInt add(const BigInt&, const BigInt&);

  static BigInt subtract(const BigInt&, const BigInt&);

  static BigInt multiply(const BigInt&, const BigInt&);

  static BigInt divide(const BigInt&, const BigInt&);

  static BigInt mod(const BigInt &lhs, const BigInt &rhs) {
    assert(rhs != 0);
    if (lhs < rhs)
      return lhs;
    
    if (lhs == rhs)
      return 0;

    if (rhs == 2)
      return !even(lhs);

    return lhs - ((lhs / rhs) * rhs);
  }

  static bool isBigint(const std::string&);

  static int countDigits(const BigInt&);

  static int digitize(const char input) {
    return input - '0';
  }

  static int characterize(const int input) {
    return input + '0';
  }

  static BigInt negate(const BigInt &input) {
    BigInt temp = input;
    temp.neg = !temp.neg;
    return temp;
  }

  static BigInt negate(BigInt&& input) {
    input.neg = !input.neg;
    return input;
  }

  static bool lessThan(const BigInt &lhs, const BigInt &rhs) {
    if (negative(lhs) && negative(rhs)) {
      return lessThan(abs(rhs), abs(lhs));
    }

    if (negative(lhs) || negative(rhs)) {
      return negative(lhs);
    }

    if (lhs.vec.size() == rhs.vec.size()) {
      return lhs.vec < rhs.vec;
    }

    return lhs.vec.size() < rhs.vec.size();
  }
};

inline bool BigInt::isBigint(const std::string &s) {
  if (s.empty())
    return false;

  // Check for leading negative sign
  if (s[0] == '-') {
    if (s.length() == 1) return false;
    // Recursively validate the rest as a positive bigint
    // Note: leading zeros check still applies to the magnitude
    return isBigint(s.substr(1));
  }

  // Check for hex string "0x..."
  if (s.length() > 2 && s[0] == '0' && s[1] == 'x') {
    return s.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos;
  }

  // Reject leading zeros for decimal numbers
  if (s.length() > 1 && s[0] == '0')
    return false;

  return s.find_first_not_of("0123456789", 0) == std::string::npos;
}

inline BigInt BigInt::add(const BigInt &lhs, const BigInt &rhs) {
  bool negate_answer = false;
  // Ensure both are positive
  if (negative(lhs) && negative(rhs)) negate_answer = true;
  else if (negative(lhs)) return subtract(rhs, abs(lhs));
  else if (negative(rhs)) return subtract(lhs, abs(rhs));

  // Ensure LHS is larger than RHS
  if (lhs.vec.size() < rhs.vec.size()) return add(rhs, lhs);

  // Prepare result vector with enough space (max size + 1 for potential carry)
  std::vector<long long> result;
  result.reserve(lhs.vec.size() + 1);

  long long carry = 0;
  auto it_l = lhs.vec.rbegin();
  auto it_r = rhs.vec.rbegin();

  while (it_l != lhs.vec.rend()) {
    long long sum = *it_l + carry;
    if (it_r != rhs.vec.rend()) {
      sum += *it_r;
      ++it_r;
    }

    if (sum >= MAX_SIZE) {
      sum -= MAX_SIZE;
      carry = 1;
    }
    else {
      carry = 0;
    }

    result.push_back(sum);
    ++it_l;
  }

  if (carry > 0) {
    result.push_back(carry);
  }

  std::reverse(result.begin(), result.end());
  BigInt result_bigint{std::move(result)};
  return negate_answer ? negate(result_bigint) : result_bigint;
}

inline BigInt BigInt::subtract(const BigInt &lhs, const BigInt &rhs) {
  // Ensure LHS is larger than RHS, and both are positive
  // (-A) - (-B) == B - A
  if (negative(lhs) && negative(rhs)) {
    return subtract(abs(rhs), abs(lhs));
  }
  // (A) - (-B) == A + B
  if (negative(rhs)) {
    return add(lhs, abs(rhs));
  }
  // (-A) - (B) == -(A + B)
  if (negative(lhs)) {
    return negate(add(abs(lhs), rhs));
  }
  if (lhs < rhs) {
    return negate(subtract(rhs, lhs));
  }

  std::vector<long long> result;
  result.reserve(lhs.vec.size());
  long long borrow = 0;

  auto it_l = lhs.vec.rbegin();
  auto it_r = rhs.vec.rbegin();

  while (it_l != lhs.vec.rend()) {
    const long long l_val = *it_l;
    const long long r_val = (it_r != rhs.vec.rend()) ? *it_r : 0;

    long long diff = l_val - r_val - borrow;
    if (diff < 0) {
      diff += MAX_SIZE;
      borrow = 1;
    }
    else {
      borrow = 0;
    }
    result.push_back(diff);

    ++it_l;
    if (it_r != rhs.vec.rend()) ++it_r;
  }
  std::reverse(result.begin(), result.end());
  return trim(BigInt(std::move(result)));
}

inline BigInt BigInt::multiply(const BigInt &lhs, const BigInt &rhs) {
  if (lhs == 0 || rhs == 0) return 0;
  if (lhs == 1) return rhs;
  if (rhs == 1) return lhs;

  if (negative(lhs) && negative(rhs)) {
    return (abs(lhs) * abs(rhs));
  }
  if (negative(lhs) || negative(rhs)) {
    return negate(abs(lhs) * abs(rhs));
  }
  if (lhs < rhs) {
    return multiply(rhs, lhs);
  }

  std::vector<long long> result(lhs.vec.size() + rhs.vec.size(), 0);

  for (auto it_lhs = lhs.vec.rbegin(); it_lhs != lhs.vec.rend(); ++it_lhs) {
    for (auto it_rhs = rhs.vec.rbegin(); it_rhs != rhs.vec.rend(); ++it_rhs) {
      // Calculate the product and the corresponding indices in the result vector
      long long mul = (*it_lhs) * (*it_rhs);
      auto pos_low_it = result.rbegin() + (std::distance(lhs.vec.rbegin(), it_lhs) + std::distance(
                            rhs.vec.rbegin(), it_rhs));
      auto pos_high_it = pos_low_it + 1;

      // Add the product to the result vector
      *pos_low_it += mul % MAX_SIZE;
      if (pos_high_it != result.rend()) {
        *pos_high_it += mul / MAX_SIZE;
      }

      // Handle carry
      if (*pos_low_it >= MAX_SIZE) {
        if (pos_high_it != result.rend()) {
          *pos_high_it += *pos_low_it / MAX_SIZE;
        }
        *pos_low_it %= MAX_SIZE;
      }
    }
  }

  // Handle carries for remaining positions
  for (auto r_iter = result.rbegin(); r_iter != result.rend() - 1; ++r_iter) {
    if (*r_iter >= MAX_SIZE) {
      *(r_iter + 1) += *r_iter / MAX_SIZE;
      *r_iter %= MAX_SIZE;
    }
  }

  return trim(result);
}


inline BigInt BigInt::divide(const BigInt &numerator, const BigInt &denominator) {
  assert(denominator != 0);
  if (numerator == denominator) return 1;
  if (denominator == 1) return numerator;
  if (numerator == 0) return 0;

  if (negative(numerator) && negative(denominator))
    return divide(abs(numerator), abs(denominator));
  
  if (negative(numerator) || negative(denominator))
    return negate(divide(abs(numerator), abs(denominator)));
  
  if (numerator < denominator)
    return 0;

  if (numerator.vec.size() <= 1)
    return numerator.vec.back() / denominator.vec.back();

  BigInt remainder = numerator;
  BigInt quotient = 0;

  auto count = countDigits(remainder) - countDigits(denominator) - 1;

  auto numerator_size = pow(10, count);

  auto temp = denominator * numerator_size;

  while (denominator * numerator_size < remainder) {
    temp = denominator * numerator_size;
    remainder -= temp;
    quotient += numerator_size;
    count = countDigits(remainder) - countDigits(denominator) - 1;


    if (numerator_size <= 1) {
      quotient += remainder / denominator;
      break;
    }
    if (remainder.vec.size() <= 1) {
      quotient += remainder.vec.back() / denominator.vec.back();
      break;
    }
    numerator_size = pow(10, count);
  }

  return quotient;
}

inline BigInt BigInt::sqrt(const BigInt &input) {
  assert(!negative(input));
  if (input == 0 || input == 1)
    return input;

  BigInt oom = log10(input / 2);
  oom /= 2;
  BigInt low_end = pow(10, (oom));
  BigInt high_end = pow(10, (oom) + 2);
  BigInt mid_point, square, answer;
  while (low_end <= high_end) {
    mid_point = (low_end + high_end) / 2;
    square = mid_point * mid_point;
    if (square == input)
      return mid_point;

    if (square < input) {
      low_end = mid_point + 1;
      answer = mid_point;
    }
    else {
      high_end = mid_point - 1;
    }
  }
  return answer;
}

inline BigInt BigInt::log2(const BigInt &input) {
  assert(input > 0);
  if (input == 1)
    return 0;

  if (input.vec.size() == 1)
    return std::log2(input.vec.back());

  BigInt exponent = 0;
  while (pow(2, exponent) <= input)
    ++exponent;

  return exponent;
}

inline BigInt BigInt::log10(const BigInt &input) {
  assert(input > 0);
  if (input == 1)
    return 0;

  if (input.vec.size() == 1) {
    return std::log10(input.vec.back());
  }
  int count = 0;
  for (auto number : input.vec) {
    count += countDigits(number);
  }

  return count - 1;
}

inline BigInt BigInt::logwithbase(const BigInt &input, const BigInt &base) {
  auto top = log2(input);
  auto bottom = log2(base);
  auto answer = divide(top, bottom);
  return answer;
}

inline BigInt BigInt::antilog2(const BigInt &input) {
  return pow(2, input);
}

inline BigInt BigInt::antilog10(const BigInt &input) {
  return pow(10, input);
}

inline void BigInt::swap(BigInt &lhs, BigInt &rhs) {
  const BigInt temp = lhs;
  lhs = rhs;
  rhs = temp;
}

inline BigInt BigInt::gcd(const BigInt &lhs, const BigInt &rhs) {
  BigInt temp_l = lhs, temp_r = rhs, remainder;
  if (rhs > lhs)
    swap(temp_l, temp_r);

  while (temp_r > 0) {
    remainder = temp_l % temp_r;
    temp_l = temp_r;
    temp_r = remainder;
  }
  return temp_l;
}

inline BigInt BigInt::factorial(const BigInt &input) {
  assert(!negative(input));
  if (input == 0)
    return 1;

  BigInt ans = 1;
  BigInt temp = input;
  while (temp != 0) {
    ans *= temp;
    temp -= 1;
  }
  return ans;
}

/* Simplest form of prime checking, implement your own
  */
inline bool BigInt::prime(const BigInt &s) {
  if (negative(s) || s == 1)
    return false;

  if (s == 2 || s == 3 || s == 5)
    return true;

  if (even(s) || s % 5 == 0)
    return false;

  for (BigInt i = 3; i * i <= s; i += 2) {
    if ((s % i) == 0) {
      return false;
    }
  }
  return true;
}

inline BigInt BigInt::random(const size_t length) {
  constexpr char charset[] = "0123456789";
  std::default_random_engine rng(std::random_device{}());

  // Distribution for the first digit (1-9)
  std::uniform_int_distribution<> first_dist(1, 9);
  // Distribution for the other digits (0-9)
  std::uniform_int_distribution<> dist(0, 9);

  // Lambda to generate the first character
  auto randchar_first = [&first_dist, &rng]() { return charset[first_dist(rng)]; };
  // Lambda to generate subsequent characters
  auto randchar = [&dist, &rng]() { return charset[dist(rng)]; };

  // Create a string with the specified length
  std::string str(length, 0);

  // Generate the first character separately to ensure it's not '0'
  str[0] = randchar_first();

  // Generate the remaining characters
  std::generate_n(str.begin() + 1, length - 1, randchar);

  return {str};
}

inline std::vector<long long> BigInt::str2vec(std::string input) {
  // Break into chunks of 9 characters
  std::vector<long long> result;
  constexpr int chunk_size = 9;
  const int size = input.size();

  if (size > chunk_size) {
    // Pad the length to get appropriate sized chunks
    if (int mod = size % chunk_size; mod != 0)
      input.insert(0, chunk_size - mod, '0');
  }
  for (unsigned i = 0; i < input.size(); i += chunk_size) {
    std::string temp_str = input.substr(i, chunk_size);
    result.emplace_back(stoll(temp_str));
  }

  return result;
}

inline std::vector<long long> BigInt::hex2vec(std::string input) {
  // Strip "0x" prefix
  const std::string hex = input.substr(2);

  std::vector<long long> result = {0};

  for (const char c : hex) {
    unsigned long long digit;
    if (c >= '0' && c <= '9')    digit = c - '0';
    else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    else assert(false && "invalid character");

    // Multiply current value by 16 and add digit.
    // Max intermediate value: (MAX_SIZE-1)*16+15 = 15,999,999,999
    // which fits in unsigned long long.
    unsigned long long carry = digit;
    for (int i = static_cast<int>(result.size()) - 1; i >= 0; --i) {
      const unsigned long long val = static_cast<unsigned long long>(result[i]) * 16 + carry;
      result[i] = static_cast<long long>(val % static_cast<unsigned long long>(MAX_SIZE));
      carry = val / static_cast<unsigned long long>(MAX_SIZE);
    }
    // carry <= 15 < MAX_SIZE, so this loop runs at most once
    while (carry > 0) {
      result.insert(result.begin(), static_cast<long long>(carry % static_cast<unsigned long long>(MAX_SIZE)));
      carry /= static_cast<unsigned long long>(MAX_SIZE);
    }
  }

  return result;
}

inline std::string BigInt::vec2str(const std::vector<long long> &input) {
  std::stringstream ss;
  bool first = true;
  for (auto partial : input) {
    if (first) {
      ss << partial; // No padding for the first number
      first = false;
    }
    else {
      ss << std::setw(9) << std::setfill('0') << partial; // Pad to 9 digits
    }
  }
  return ss.str();
}

inline int BigInt::countDigits(const BigInt &input) {
  std::string my_string = vec2str(input.vec);
  return static_cast<int>(my_string.length()) - 1;
}

inline BigInt gcd(const BigInt &a, const BigInt &b) {
  return BigInt::gcd(a, b);
}

inline BigInt abs(const BigInt &a) {
  return BigInt::abs(a);
}

using std::gcd;
using std::abs;

}

template<>
struct std::hash<pres::BigInt> {
  std::size_t operator()(const pres::BigInt &input) const noexcept {
    std::size_t seed = input.vec.size();
    for (auto x : input.vec) {
      x = ((x >> 16) ^ x) * 0x45d9f3b;
      x = ((x >> 16) ^ x) * 0x45d9f3b;
      x = (x >> 16) ^ x;
      seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    if (input.neg) {
      seed ^= 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
  }
};

#endif /* BIGINT_H_ */
