#ifndef META_H
#define META_H

#include <string>
#include <iostream>

namespace meta {

template<int N>
struct const_str {
  char data[N + 1];

  constexpr const_str() {
    for (int i = 0; i <= N; i++)
      data[i] = 0;
  }
  constexpr const_str(const const_str &other) {
    for (int i = 0; i <= N; i++)
      data[i] = other.data[i];
  }

  operator const char *() { return data; }
  operator std::string() { return data; }
};

template<class T>
constexpr auto name() {
  constexpr std::string_view function = __PRETTY_FUNCTION__;
  constexpr auto from = function.rfind("T = ") + 4;
  static_assert(from != std::string_view::npos);
  constexpr auto to = function.rfind("]");
  static_assert(to != std::string_view::npos && to > from);
  const_str<to - from + 1> arr;
  for (auto i = from; i < to; i++)
    arr.data[i - from] = function[i];
  arr.data[to - from] = '\0';
  return arr;
}

template<int N>
std::ostream &operator<<(std::ostream &os, const const_str<N> &str) {
  return os << str.data;
}

template<class F>
struct InfixFunction {
  F f;
};

template<class T, class F>
struct InfixProxy {
  F f;
  T lhs;
};

template<class T, class F>
auto operator<(T &&lhs, InfixFunction<F> op) {
  return InfixProxy<T, F>{ std::forward<T>(lhs), op.f };
}

template<class T, class F, class U>
decltype(auto) operator>(InfixProxy<T, F> &&proxy, U &&rhs) {
  return proxy.f(std::forward<U>(proxy.lhs), std::forward<U>(rhs));
}

template<class F>
constexpr auto infix(F &&f) {
  return InfixFunction<std::decay_t<F>>{ std::forward<F>(f) };
}

}

#if __cplusplus >= 202002L
# define __requires(x) requires(x)
#else
# define __requires(x)
#endif

#endif
