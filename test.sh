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
  -p|--print-before)
    if [[ $# -lt 2 ]]; then
      die "expected test case name"
    fi
    printbefore="$2"
    shift 2;;
  -q|--print-after)
    if [[ $# -lt 2 ]]; then
      die "expected test case name"
    fi
    printafter="$2"
    shift 2;;
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
ret=$?
if [[ $ret -ne 0 ]]; then
  die "compile error: make returned $ret"
fi
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
  cmd="build/hcc $name"
  if [[ -n $gdb ]]; then
    cmd="gdb --args $cmd"
  fi
  if [[ -n $printbefore ]]; then
    cmd="$cmd --print-before $printbefore"
  fi
  if [[ -n $printafter ]]; then
    cmd="$cmd --print-after $printafter"
  fi
  eval "ASAN_OPTIONS=detect_leaks=0 $cmd"
  ret=$?
  if [[ $ret -ne 0 ]]; then
    die "hcc error: returned $ret"
  fi
fi
