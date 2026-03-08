#!/bin/python
import subprocess
import time
import os
import sys
from pathlib import Path

TEST_ROOT = Path("test")
TEMP_DIR = Path("temp")
COMPILER = "build/hcc" 
QEMU = "qemu-aarch64-static"
RESULT_FILE = Path("results.json")
GCC = "aarch64-linux-gnu-gcc"

# Remove trailing spaces on each line.
def normalize(text: str):
  return "\n".join(line.rstrip() for line in text.splitlines())

def run_test(test_path: Path) -> tuple[bool, None | float]:
  name = test_path.stem
  asm_path = TEMP_DIR / (name + ".s")
  exe_path = TEMP_DIR / (name + ".exe")

  hcc = subprocess.run(
    [COMPILER, str(test_path), "-o", str(asm_path)],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
    env={"ASAN_OPTIONS": "detect_leaks=0"}
  )

  if hcc.returncode != 0:
    print(f"hcc error: {hcc.returncode}")
    try: os.remove(asm_path)
    except: ...
    return False, None
  
  # c test/lib.c -x assembler $output -o temp/a.out -static
  gcc = subprocess.run(
    [GCC, "-x", "c", "test/lib.c", "-x", "assembler", asm_path, "-o", exe_path, "-static"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
  )
  if gcc.returncode != 0:
    print(f"gcc error: {gcc.returncode}")
    try: os.remove(exe_path)
    except: ...
    return False, None

  os.remove(asm_path)

  input_file = test_path.with_suffix(".in")
  input = None
  if input_file.exists():
    input = input_file.read_text()

  start = time.perf_counter()
  qemu = subprocess.run(
    [QEMU, str(exe_path)],
    input=input,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
  )
  end = time.perf_counter()
  os.remove(exe_path)

  elapsed = end - start

  # 4. Load expected output
  expected_file = test_path.with_suffix(".out")
  expected_lines = expected_file.read_text().splitlines()

  expected_return = int(expected_lines[-1])
  expected_stdout = "\n".join(expected_lines[:-1])

  actual_stdout = qemu.stdout
  actual_return = qemu.returncode

  # Normalize trailing spaces
  expected_stdout = normalize(expected_stdout)
  actual_stdout = normalize(actual_stdout)

  if expected_stdout != actual_stdout:
    print(f"expected: {expected_stdout}")
    print(f"got: {actual_stdout}")
    return False, None
  
  if expected_return != actual_return:
    print(f"expected return: {expected_return}")
    print(f"got: {actual_return}")
    return False, None

  return True, elapsed

timemap = {}
for file in TEST_ROOT.rglob("*.sy"):
  name = str(file)
  if "performance" in name or "h_functional" in name or "custom" in name:
    continue
  print(f"running: {file}", file=sys.stderr)

  passed, elapsed = run_test(file)
  if not passed:
    print("error!", file=sys.stderr)
  if elapsed:
    timemap[file.stem] = elapsed

if len(timemap):
  print(timemap, file=sys.stderr)

run_test(Path("test/functional/16_mulc.sy"))
