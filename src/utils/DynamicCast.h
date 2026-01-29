#ifndef DYNAMIC_CAST_H
#define DYNAMIC_CAST_H

#include <cassert>
#include "Meta.h"

template<class T, class U> __requires((std::derived_from<T, U>))
bool isa(U *t) {
  return T::classof(t);
}

template<class T, class U> __requires((std::derived_from<T, U>))
T *cast(U *t) {
  assert(isa<T>(t));
  return (T*) t;
}

template<class T, class U> __requires((std::derived_from<T, U>))
T *dyn_cast(U *t) {
  if (!isa<T>(t))
    return nullptr;
  return cast<T>(t);
}

#endif
