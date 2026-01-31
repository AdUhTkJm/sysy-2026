#ifndef PRINTER_H
#define PRINTER_H

#include "OpBase.h"
#include <unordered_map>
#include <sstream>

namespace ir {

class Printer {
  std::unordered_map<const Block *, int> blockid;
  std::unordered_map<const Value *, int> valueid;
  std::unordered_map<const Value *, std::string> idents;
  std::ostringstream os;
  std::string bbPrefix = "bb";
  int depth = 0, bid = 0, vid = 0;

  using Print = void (*)(std::ostream &, const Op *op, Printer *printer);
  using PrintMap = std::unordered_map<size_t, Printer::Print>;
  using AttrPrint = void (*)(std::ostream &os, const Attr *attr, Printer *printer);
  using AttrPrintMap = std::unordered_map<size_t, AttrPrint>;

  static PrintMap &dispatch();
  static AttrPrintMap &attrDispatch();
  void indent();
  void printImpl(const Block *bb, bool tag);
public:
  Printer() = default;
  int id(const Block *block);
  int id(const Value *value);
  std::string str(const Value *value, bool isWide = false);
  std::string str(const Block *block);

  void print(const Region *region);
  void print(const Block *block);
  void print(const Op *op);
  void print(const Attr *attr);

  void printResults(const Op *op, unsigned from = 0);
  void printOperands(const Op *op, unsigned from = 0);
  void printType(const Type *type);

  void reset();
  void addIdent(const Value *v, const std::string &str) { idents[v] = str; }
  void setBlockPrefix(const std::string &prefix) { bbPrefix = prefix; }
  
  void dump(std::ostream &os);
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
