#!/bin/python
import subprocess
import time
import os
import sys
import argparse
import concurrent.futures
import json
from pathlib import Path

TEST_ROOT = Path("test")
TEMP_DIR = Path("temp")
COMPILER = "build/hcc" 
QEMU = "qemu-aarch64-static"
RESULT_FILE = Path("results.json")
GCC = "aarch64-linux-gnu-gcc"
DEFAULT_QEMU_TIMEOUT_S = 3.0

# Remove trailing spaces on each line.
def normalize(text: str):
  return "\n".join(line.rstrip() for line in text.splitlines())

def _test_temp_id(test_path: Path) -> str:
  """
  Create a unique id for temp artifacts.
  Using relative path avoids collisions when different folders share a stem.
  """
  rel = test_path.relative_to(TEST_ROOT)
  return rel.as_posix().replace("/", "__")

def run_test(test_path: Path, *, qemu_timeout_s: float) -> tuple[bool, None | float, str]:
  asm_path = TEMP_DIR / (_test_temp_id(test_path) + ".s")
  exe_path = TEMP_DIR / (_test_temp_id(test_path) + ".exe")

  def _truncate(s: str, n: int = 200) -> str:
    return (s[:n] + "...") if len(s) > n else s

  def _cleanup():
    for p in (asm_path, exe_path):
      try:
        if p.exists():
          os.remove(p)
      except OSError:
        ...

  try:
    hcc = subprocess.run(
      [COMPILER, str(test_path), "-o", str(asm_path)],
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      text=True,
      env={"ASAN_OPTIONS": "detect_leaks=0"},
    )

    if hcc.returncode != 0:
      return False, None, f"hcc error (rc={hcc.returncode}): {test_path}"

    # c test/lib.c -x assembler $output -o temp/a.out -static
    gcc = subprocess.run(
      [GCC, "-x", "c", "test/lib.c", "-x", "assembler", asm_path, "-o", exe_path, "-static"],
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      text=True,
    )
    if gcc.returncode != 0:
      return False, None, f"gcc error (rc={gcc.returncode}): {test_path}"

    try:
      if asm_path.exists():
        os.remove(asm_path)
    except OSError:
      ...

    # Optional stdin
    input_file = test_path.with_suffix(".in")
    qemu_input = input_file.read_text() if input_file.exists() else None

    qemu_start = time.perf_counter()
    try:
      qemu = subprocess.run(
        [QEMU, str(exe_path)],
        input=qemu_input,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        timeout=qemu_timeout_s,
      )
    except subprocess.TimeoutExpired:
      return False, None, f"timeout after {qemu_timeout_s}s: {test_path}"
    finally:
      try:
        if exe_path.exists():
          os.remove(exe_path)
      except OSError:
        ...

    elapsed = time.perf_counter() - qemu_start

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
      return (
        False,
        None,
        f"stdout mismatch: {test_path}\nexpected(first 200): {_truncate(expected_stdout)}\ngot(first 200): {_truncate(actual_stdout)}",
      )

    if expected_return != actual_return:
      return False, None, f"return code mismatch (expected {expected_return}, got {actual_return}): {test_path}"

    return True, elapsed, f"ok: {test_path}"
  finally:
    _cleanup()

def find_tests(dir: str) -> list[Path]:
  tests: list[Path] = []
  for file in TEST_ROOT.rglob("*.sy"):
    name = str(file)
    if not f"/{dir}/" in name:
      continue
    tests.append(file)
  return tests

def main() -> int:
  parser = argparse.ArgumentParser(description="Run functional .sy tests")
  parser.add_argument("--jobs", type=int, default=min(32, os.cpu_count() or 1), help="Parallel worker count")
  parser.add_argument(
    "--timeout",
    type=float,
    default=DEFAULT_QEMU_TIMEOUT_S,
    help="Per-test QEMU timeout in seconds",
  )
  parser.add_argument("--directory", "-d", type=str, default="functional")
  parser.add_argument("--max-tests", type=int, default=0, help="If >0, only run first N tests")
  args = parser.parse_args()

  TEMP_DIR.mkdir(parents=True, exist_ok=True)

  tests = find_tests(args.directory)
  if args.max_tests and args.max_tests > 0:
    tests = tests[: args.max_tests]

  if not tests:
    print("No tests found.", file=sys.stderr)
    return 0

  timemap: dict[str, float] = {}
  failures: list[str] = []

  with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
    futures: dict[concurrent.futures.Future, Path] = {}
    for t in tests:
      fut = ex.submit(run_test, t, qemu_timeout_s=args.timeout)
      futures[fut] = t

    for fut in concurrent.futures.as_completed(futures):
      t = futures[fut]
      try:
        passed, elapsed, msg = fut.result()
      except Exception as e:
        passed = False
        elapsed = None
        msg = f"exception: {e} ({t})"

      if passed:
        if elapsed is not None:
          timemap[t.stem] = float(elapsed)
      else:
        failures.append(msg)
        print(f"\nFAIL: {t}\n{msg}", file=sys.stderr)

  if failures:
    print(f"\n{len(failures)} test(s) failed.", file=sys.stderr)

  if len(timemap):
    timemap = { k: f"{v:.4f}" for k, v in timemap.items() }
    print(json.dumps(timemap, indent=2), file=sys.stderr)
  return 0

if __name__ == "__main__":
  raise SystemExit(main())
