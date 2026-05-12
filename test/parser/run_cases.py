#!/usr/bin/env python3

from __future__ import annotations

import difflib
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DRIVER_PATH = SCRIPT_DIR / "stack-parser-driver"
CASES_DIR = SCRIPT_DIR / "cases"
RESULT_PATH = SCRIPT_DIR / "result.txt"


def collect_cases(cases_dir: Path) -> list[tuple[Path, Path]]:
    cases = []
    for binary_path in sorted(cases_dir.glob("*.bin")):
        expected_path = binary_path.with_suffix(".mlir")
        if not expected_path.exists():
            raise FileNotFoundError(f"missing expected file for {binary_path.name}: {expected_path.name}")
        cases.append((binary_path, expected_path))
    if not cases:
        raise FileNotFoundError(f"no .bin cases found in {cases_dir}")
    return cases


def normalize_text(text: str) -> str:
    lines = [line.strip() for line in text.splitlines()]
    return "\n".join(lines).rstrip("\n") + "\n"


def write_results(result_path: Path, summaries: list[str]) -> None:
    if summaries:
        result_path.write_text("\n".join(summaries) + "\n")
    else:
        result_path.write_text("")


def summarize_case(binary_path: Path, passed: bool) -> str:
    encoded = "".join(binary_path.read_text().split())
    status = "pass" if passed else "fail"
    return f"[{binary_path.name}: {encoded} --{status}]"


def run_case(driver_path: Path, binary_path: Path, expected_path: Path) -> bool:
    print(f"TEST {binary_path}")
    try:
        proc = subprocess.run(
            [str(driver_path), str(binary_path)],
            capture_output=True,
            text=True,
            check=False,
        )
    except Exception as ex:
        print(f"FAIL {binary_path.stem}: runner exception: {ex}", file=sys.stderr)
        return False

    expected = normalize_text(expected_path.read_text())
    if expected.strip() == "FAIL":
        if proc.returncode != 0:
            print(f"PASS {binary_path.stem}")
            return True

        print(f"FAIL {binary_path.stem}: expected compiler failure", file=sys.stderr)
        return False

    actual = normalize_text(proc.stdout)

    if proc.returncode != 0:
        print(f"FAIL {binary_path.stem}: driver exited with {proc.returncode}", file=sys.stderr)
        if proc.stderr:
            print(proc.stderr, file=sys.stderr, end="" if proc.stderr.endswith("\n") else "\n")
        return False

    if actual != expected:
        print(f"FAIL {binary_path.stem}: output mismatch", file=sys.stderr)
        diff = difflib.unified_diff(
            expected.splitlines(keepends=True),
            actual.splitlines(keepends=True),
            fromfile=str(expected_path),
            tofile="actual",
        )
        sys.stderr.writelines(diff)
        if proc.stderr:
            print(proc.stderr, file=sys.stderr, end="" if proc.stderr.endswith("\n") else "\n")
        return False

    print(f"PASS {binary_path.stem}")
    return True


def main() -> int:
    # collect all test cases
    cases = collect_cases(CASES_DIR)
    ok = True
    summaries = []
    try:
        for binary_path, expected_path in cases:
            # run the case
            passed = run_case(DRIVER_PATH, binary_path, expected_path)
            # write to summary
            summaries.append(summarize_case(binary_path, passed))
            write_results(RESULT_PATH, summaries)
            ok = passed and ok
    finally:
        write_results(RESULT_PATH, summaries)

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())