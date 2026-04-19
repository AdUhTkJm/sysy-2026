#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#include <cstddef>
#include <utility>
#include <iterator>
#include <vector>
#include <iostream>

namespace data {

template<class T>
class Enumerator {
  const T &t;
public:
  explicit Enumerator(const T &t): t(t) {}

  struct iterator {
    size_t index;
    decltype(std::begin(std::declval<const T&>())) iter;

    bool operator!=(const iterator& other) const {
      return iter != other.iter;
    }

    void operator++() {
      ++index;
      ++iter;
    }

    auto operator*() const {
      return std::pair<std::size_t, decltype(*iter)>(index, *iter);
    }
  };

  iterator begin() {
    return { 0, std::begin(t) };
  }

  iterator end() {
    return { 0, std::end(t) };
  }
};

template<class T>
auto enumerate(const T &t) {
  return Enumerator<T>(t);
}

template<class T>
void concat(std::vector<T> &a, const std::vector<T> &b) {
  a.reserve(b.size() + a.size());
  for (auto x : b)
    a.push_back(x);
}

template<class T>
class Reverser {
  const T &t;
public:
  explicit Reverser(const T &t): t(t) {}
  using iterator = decltype(std::rbegin(std::declval<const T&>()));

  iterator begin() { return std::rbegin(t); }
  iterator end() { return std::rend(t); }
};

template<class T>
auto reverse(const T &t) {
  return Reverser<T>(t);
}

}

#endif
