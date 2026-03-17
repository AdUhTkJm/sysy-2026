#include "Alloc.h"
#include <cassert>
#include <algorithm>

Arena::Arena(): buf(new char[chunk]), head(buf), used(sizeof(char*)) {
  *(char **) buf = 0;
}

Arena::~Arena() {
  reset();
}

void *Arena::allocate(size_t request, size_t align) {
  assert(request + sizeof(char *) < chunk);
  used = (used + align - 1) & ~(align - 1);

  // We need to allocate a new chunk.
  if (request + used > chunk) {
    auto v = new char[chunk];
    *(char **) buf = v;
    *(char **) v = 0;
    used = std::max(align, sizeof(char*));
    buf = v;
  }
  
  auto result = buf + used;
  used += request;
  return result;
}

void Arena::reset() {
  for (char *p = head; p;) {
    auto q = *(char **) p;
    delete[] p;
    p = q;
  }
  buf = new char[chunk];
  *(char **) buf = 0;
  head = buf;
  used = sizeof(char*);
}
