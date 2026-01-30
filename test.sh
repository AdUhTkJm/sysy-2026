#!/bin/zsh

die() {
  echo -e "\e[31merror:\e[0m $1"
  exit 1
}
warn() {
  echo -e "\e[33mwarning:\e[0m $1"
}

while [[ $# -gt 0 ]] do
  case "$1" in
  -r|--rebuild)
    rm -rf build
    shift;;
  -t|--test)
    if [[ $# -lt 2 ]]; then
      die "expected test case name"
    fi
    v=3
    case "$2" in
    f) testdir=functional; testcase=$3;;
    h) testdir=h_functional; testcase=$3;;
    p) testdir=performance; testcase=$3;;
    *) testcase=$2; v=2;;
    esac
    shift $v;;
  -g|--gdb)
    gdb=1; shift;;
  *)
    die "unknown name: $1";;
  esac
done

# Rebuild.
if [[ ! -d $build ]]; then
  cmake -S . -B build
fi
cd build
make -j$(nproc)
cd ..

if [[ -n $testcase ]]; then
  # Supply the leading zero.
  if [[ $testcase -ge 0 && $testcase -le 9 && $(echo $testcase | wc -c) -eq 2 ]]; then
    testcase=0$testcase
  fi
  # Find the test case.
  name=$(find test/$testdir -regex ".*/$testcase.*sy")
  if [[ -z $name ]] then
    die "no file: $testcase"
  fi
  if [[ $(echo $name | wc -l) -gt 1 ]]; then
    die "ambiguous name: $testcase"
  fi
  echo "running: $name"
  if [[ -n $gdb ]]; then
    gdb --args build/hcc $name
  else
    build/hcc $name
  fi
  ret=$?
  if [[ $ret -ne 0 ]]; then
    die "hcc error: returned $ret"
  fi
fi
