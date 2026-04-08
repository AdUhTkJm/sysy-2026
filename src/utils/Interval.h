#ifndef INTERVAL_H
#define INTERVAL_H

#include <algorithm>
#include <climits>
#include <unordered_map>
#include <iostream>

namespace ir {

class Value;

}

namespace data {

struct Interval {
  int lo, hi;

  static Interval top() { return { INT_MIN, INT_MAX }; }
  Interval(): lo(INT_MIN), hi(INT_MAX) {}
  Interval(int lo, int hi): lo(lo), hi(hi) {}
  Interval(int c): lo(c), hi(c) {}

  bool operator==(Interval other) const {
    return lo == other.lo && hi == other.hi;
  }

  Interval join(Interval other) const {
    return {
      std::min(lo, other.lo),
      std::max(hi, other.hi)
    };
  }

  Interval intersect(Interval other) const {
    return {
      std::max(lo, other.lo),
      std::min(hi, other.hi)
    };
  }

};

Interval operator+(Interval a, Interval b);
Interval operator-(Interval a, Interval b);
Interval operator*(Interval a, Interval b);
Interval operator/(Interval a, Interval b);
Interval operator%(Interval a, Interval b);

std::ostream& operator<<(std::ostream &os, Interval i);

struct Env {
  using Data = std::unordered_map<const ir::Value*, Interval>;
  Data *data = new Data();

  Interval &operator[](const ir::Value *v) { return (*data)[v]; }

  Interval &at(const ir::Value *v) { return data->at(v); }
  const Interval &at(const ir::Value *v) const { return data->at(v); }

  Env join(const Env &other) const {
    Env out = clone();
    for (auto& [v, i] : *other.data)
      out[v] = out[v].join(i);
    return out;
  }

  Env clone() const {
    return { new Data(*data) };
  }
};

struct ConstEnv {
  const Env::Data *const data;

  Interval operator[](const ir::Value *v) const { return data->at(v); }
  Interval at(const ir::Value *v) const { return data->at(v); }
  int count(const ir::Value *v) const { return data->count(v); }

  ConstEnv(Env x): data(x.data) {}
  ConstEnv(const ConstEnv &other): data(other.data) {}
};

}

#endif
