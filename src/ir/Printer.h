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
  const Op *printing = nullptr;

  using Print = void (*)(std::ostream &, const Op *op, Printer *printer);
  using PrintMap = std::unordered_map<size_t, Printer::Print>;
  using AttrPrint = void (*)(std::ostream &os, const Attr *attr, Printer *printer);
  using AttrPrintMap = std::unordered_map<size_t, AttrPrint>;

  static PrintMap &dispatch();
  static AttrPrintMap &attrDispatch();
  void indent(std::ostream &os);
  void printImpl(const Block *bb, bool tag);
public:
  Printer() = default;
  int id(const Block *block);
  int id(const Value *value);
  std::string str(const Value *value);
  std::string str(const Block *block);
  bool showHidden = true;
  bool showAttr = true;
  bool showRange = true;

  void print(const Region *region);
  void print(const Block *block);
  void print(const Op *op);
  void print(const Attr *attr);

  void printResults(std::ostream &os, const Op *op, unsigned from = 0);
  void printOperands(std::ostream &os, const Op *op, unsigned from = 0);
  void printType(std::ostream &os, const Type *type);
  void printNewline(std::ostream &os);

  void reset();
  void addIdent(const Value *v, const std::string &str) { idents[v] = str; }
  void setBlockPrefix(const std::string &prefix) { bbPrefix = prefix; }
  void setIndent(int i) { depth = i; }
  
  void dump(std::ostream &os);
} extern printer;

template<class T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
  if (vec.empty())
    return os << "<empty>";
  
  for (size_t i = 0; i < vec.size(); i++) {
    os << vec[i];
    if (i != vec.size() - 1)
      os << ", ";
  }
  return os;
}
std::ostream &operator<<(std::ostream &os, const Op *op);
std::ostream &operator<<(std::ostream &os, const Value *v);
std::ostream &operator<<(std::ostream &os, const Block *bb);
std::ostream &operator<<(std::ostream &os, const Region *region);
std::ostream &operator<<(std::ostream &os, const Type *type);

}

#endif
