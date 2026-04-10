#include "Interval.h"
#include <cassert>
#include <optional>

namespace data {

static std::optional<int> safe_add(int a, int b) {
  int result;
  if (__builtin_add_overflow(a, b, &result))
    return std::nullopt;
  return result;
}

static std::optional<int> safe_sub(int a, int b) {
  int result;
  if (__builtin_sub_overflow(a, b, &result))
    return std::nullopt;
  return result;
}

static std::optional<int> safe_mul(int a, int b) {
  int result;
  if (__builtin_mul_overflow(a, b, &result))
    return std::nullopt;
  return result;
}

Interval operator+(Interval a, Interval b) {
  auto c = safe_add(a.lo, b.lo);
  if (!c)
    c = INT_MIN;

  auto d = safe_add(a.hi, b.hi);
  if (!d)
    d = INT_MAX;

  return Interval(*c, *d);
}

Interval operator-(Interval a, Interval b) {
  auto c = safe_sub(a.lo, b.hi);
  if (!c)
    c = INT_MIN;

  auto d = safe_sub(a.hi, b.lo);
  if (!d)
    d = INT_MAX;

  return Interval(*c, *d);
}

Interval operator*(Interval a, Interval b) {
  int as[2] = { a.lo, a.hi };
  int bs[2] = { b.lo, b.hi };
  int min = INT_MAX, max = INT_MIN;

  for (int x : as) {
    for (int y : bs) {
      auto c = safe_mul(x, y);
      if (!c)
        return Interval();
      
      int v = *c;
      if (v < min)
        min = v;
      if (v > max) 
        max = v;
    }
  }
  return { min, max };
}

Interval operator/(Interval a, Interval b) {
  // When the sign of `b` is unknown, we split it into two halves.
  if (b.lo < 0 && b.hi > 0)
    return (a / Interval(b.lo, -1)).join(a / Interval(1, b.hi));
  
  long as[2] = { a.lo, a.hi };
  long bs[2] = { b.lo, b.hi };
  long min = INT_MAX, max = INT_MIN;

  for (long x : as) {
    for (long y : bs) {
      long v = x / y;
      if (v < min)
        min = v;
      if (v > max) 
        max = v;
    }
  }
  return { min < INT_MIN ? INT_MIN : int(min), max > INT_MAX ? INT_MAX : int(max) };
}

Interval operator%(Interval a, Interval b) {
  auto l = std::abs(b.lo), r = std::abs(b.hi);
  auto ub = std::max(l, r) - 1;
  if (std::abs(a.hi) < ub && std::abs(a.lo) < ub)
    return a;
  if (a.lo >= 0)
    return Interval(0, ub);
  if (a.hi <= 0)
    return Interval(-ub, 0);

  return Interval(-ub, ub);
}


std::ostream& operator<<(std::ostream &os, Interval i) {
  if (i.hi == INT_MAX && i.lo == INT_MIN)
    return os << "[⊤]";
  if (i.hi == i.lo)
    return os << "[" << i.hi << "]";
  if (i.hi < i.lo)
    return os << "[∅]";
  if (i.hi == INT_MAX)
    return os << "[" << i.lo << ", +]";
  return os << "[" << i.lo << ", " << i.hi << "]";
}

}
