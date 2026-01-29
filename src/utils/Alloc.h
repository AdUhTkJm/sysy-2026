#ifndef ALLOC_H
#define ALLOC_H

#include <cstddef>

class Arena {
  char *buf, *head;
  size_t used;
  constexpr static size_t chunk = 1048576;
public:
  Arena();
  void *allocate(size_t request, size_t align);
  void reset();
};

#endif
