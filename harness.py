#!/bin/python
import subprocess
import time
import os
from pathlib import Path

TEST_ROOT = Path("test")
TEMP_DIR = Path("temp")
COMPILER = "build/hcc"   # your compiler
QEMU = "qemu-aarch64-static"
RESULT_FILE = Path("results.json")

# Remove trailing spaces on each line.
def normalize(text: str):
  return "\n".join(line.rstrip() for line in text.splitlines())

def run_test(test_path: Path) -> tuple[bool, None | float]:
  name = test_path.stem
  exe_path = TEMP_DIR / name

  hcc = subprocess.run(
    [COMPILER, str(test_path), "-o", str(exe_path)],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True,
    env={"ASAN_OPTIONS": "detect_leaks=0"}
  )

  if hcc.returncode != 0:
    print(f"hcc error: {hcc.returncode}")
    os.remove(exe_path)
    return False, None

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

  passed = (
    expected_stdout == actual_stdout and
    expected_return == actual_return
  )
  if not passed:
    print(expected_stdout)
    print(actual_stdout)
    print(expected_return)
    print(actual_return)

  return passed, elapsed

timemap = {}
for file in TEST_ROOT.rglob("*.sy"):
  print(f"running: {file}")
  # Temporarily disable performance for now.
  if "performance" in str(file):
    continue

  passed, elapsed = run_test(file)
  if not passed:
    print("error!")
  if elapsed:
    timemap[file.stem] = elapsed

if len(timemap):
  print(timemap)
