#include "Options.h"
#include <cstring>
#include <iostream>

#define PARSEOPT(str, field) \
  if (strcmp(argv[i], str) == 0) { \
    opts.field = true; \
    continue; \
  }

Options::Options() {
  memset((void *) this, 0, offsetof(Options, optEnd));
}

Options parseArgs(int argc, char **argv) {
  Options opts;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0) {
      opts.outputFile = argv[i + 1];
      i++;
      continue;
    }

    if (strcmp(argv[i], "--print-after") == 0) {
      opts.printAfter = argv[i + 1];
      if (i + 2 < argc && '0' <= *argv[i + 2] && *argv[i + 2] <= '9') {
        opts.afterIndex = std::atoi(argv[i + 2]);
        i++;
      }
      i++;
      continue;
    }

    if (strcmp(argv[i], "--print-before") == 0) {
      opts.printBefore = argv[i + 1];
      if (i + 2 < argc && '0' <= *argv[i + 2] && *argv[i + 2] <= '9') {
        opts.beforeIndex = std::atoi(argv[i + 2]);
        i++;
      }
      i++;
      continue;
    }
    
    if (strcmp(argv[i], "--compare") == 0) {
      opts.compareWith = argv[i + 1];
      i++;
      continue;
    }

    if (strcmp(argv[i], "-i") == 0) {
      opts.interpretInput = argv[i + 1];
      i++;
      continue;
    }

    if (strcmp(argv[i], "--unit-test") == 0) {
      opts.unitTest = argv[i + 1];
      i++;
      continue;
    }

    PARSEOPT("--dump-ast", dumpAST);
    PARSEOPT("--dump-mid-ir", dumpMidIR);
    PARSEOPT("--rv", rv);
    PARSEOPT("--arm", arm);
    PARSEOPT("-O1", o1);
    PARSEOPT("-S", noLink);
    PARSEOPT("-v", verbose);
    PARSEOPT("--stats", stats);
    PARSEOPT("-s", stats);
    PARSEOPT("--verify", verify);
    PARSEOPT("--print-all", printAll);
    PARSEOPT("--print-type", printType);
    PARSEOPT("--ranges", ranges);
    PARSEOPT("--interpret", interpret);

    if (opts.inputFile != "") {
      std::cerr << "error: multiple inputs\n";
      exit(1);
    }

    opts.inputFile = argv[i];
  }

  if (opts.rv && opts.arm) {
    std::cerr << "error: multiple target\n";
    exit(1);
  }

  if (!opts.rv && !opts.arm)
    opts.rv = true;

  return opts;
}
