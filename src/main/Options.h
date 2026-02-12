#ifndef OPTIONS_H
#define OPTIONS_H

#include <string>

struct Options {
  using option = unsigned char;

  option dumpAST;
  option noLink;
  option dumpMidIR;
  option o1;
  option arm;
  option rv;
  option verbose;
  option stats;
  option verify;
  option printType;
  option printAll;
  option interpret;

  option optEnd;

  std::string inputFile;
  std::string outputFile;
  std::string printAfter;
  std::string printBefore;
  std::string compareWith;
  std::string interpretInput;
  
  Options();
} extern options;

Options parseArgs(int argc, char **argv);

#endif
