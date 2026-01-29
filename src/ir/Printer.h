#ifndef PRINTER_H
#define PRINTER_H

#include "OpBase.h"
#include <unordered_map>

namespace ir {

class Printer {
  std::unordered_map<Block *, int> blockid;
  std::unordered_map<Value *, int> valueid;
  std::ostream &os;
  int depth = 0, bid = 0, vid = 0;

  using Print = void (*)(std::ostream &, Op *op, Printer *printer);
  using PrintMap = std::unordered_map<size_t, Printer::Print>;
  static PrintMap &dispatch();
  void indent();
  void printImpl(Block *bb, bool tag);
public:
  Printer(std::ostream &os): os(os) {}
  int id(Block *block);
  int id(Value *value);

  void print(Region *region);
  void print(Block *block);
  void print(Op *op);

  void printResults(Op *op, unsigned from = 0);
  void printOperands(Op *op, unsigned from = 0);
  void printType(const Type *type);

  void reset();
} extern printer;

template<class T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
  for (size_t i = 0; i < vec.size(); i++) {
    os << vec[i];
    if (i != vec.size() - 1)
      os << ", ";
  }
  return os;
}
std::ostream &operator<<(std::ostream &os, Op *op);
std::ostream &operator<<(std::ostream &os, Block *bb);
std::ostream &operator<<(std::ostream &os, Region *region);

}

#endif
