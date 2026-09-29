#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CXX = os.environ.get("CXX", "c++")
SANITIZER_MODE = os.environ.get("EDP_EVENT_SANITIZER_MODE", "").strip().lower()

DEPENDENCIES = (
    "EDP-System",
    "EDP-Primitives",
    "EDP-Memory",
    "EDP-Threading",
    "EDP-Clock",
    "EDP-BoundedTopology",
    "EDP-BoundedTypes",
    "EDP-Platform",
)
INCLUDES = [ROOT / "src"] + [ROOT.parent / name / "src" for name in DEPENDENCIES]

POSITIVE_TESTS = (
    ROOT / "tests" / "host" / "planner.cpp",
    ROOT / "tests" / "host" / "runtime.cpp",
    ROOT / "tests" / "host" / "layout.cpp",
    ROOT / "tests" / "host" / "concurrency.cpp",
)
COMPILE_FAIL_TESTS = tuple(sorted((ROOT / "tests" / "compile_fail").glob("*.cpp")))
PUBLIC_HEADERS = tuple(sorted((ROOT / "src" / "event").glob("*.hpp"))) + (ROOT / "src" / "ESPressio_Event.hpp",)

COMMON = [
    "-std=c++20",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Werror",
    "-pthread",
    *[f"-I{path}" for path in INCLUDES],
]

if SANITIZER_MODE:
    if SANITIZER_MODE not in {"undefined", "address"}:
        raise SystemExit(f"Unsupported EDP_EVENT_SANITIZER_MODE: {SANITIZER_MODE}")
    COMMON.extend([f"-fsanitize={SANITIZER_MODE}", f"-fno-sanitize-recover={SANITIZER_MODE}"])


def run(command: list[str], expect_success: bool) -> bool:
    result = subprocess.run(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    succeeded = result.returncode == 0
    if succeeded == expect_success:
        return True
    sys.stderr.write(result.stdout)
    sys.stderr.write(result.stderr)
    return False


def main() -> int:
    policy_checks = subprocess.run(
        [sys.executable, str(ROOT / "tests" / "run_policy_checks.py")],
        cwd=ROOT,
        check=False,
    )
    if policy_checks.returncode != 0:
        return policy_checks.returncode

    for include in INCLUDES:
        if not include.is_dir():
            print(f"ERROR: required include root missing: {include}", file=sys.stderr)
            return 2

    with tempfile.TemporaryDirectory(prefix="edp-event-tests-") as temp_name:
        temp = Path(temp_name)
        for source in POSITIVE_TESTS:
            executable = temp / source.stem
            if not run([CXX, *COMMON, str(source), "-o", str(executable)], True):
                print(f"FAIL: positive compile: {source.relative_to(ROOT)}")
                return 1
            if not run([str(executable)], True):
                print(f"FAIL: positive execution: {source.relative_to(ROOT)}")
                return 1
            print(f"PASS: {source.relative_to(ROOT)}")

        for source in COMPILE_FAIL_TESTS:
            output = temp / f"{source.stem}.o"
            if not run([CXX, *COMMON, "-c", str(source), "-o", str(output)], False):
                print(f"FAIL: negative test compiled: {source.relative_to(ROOT)}")
                return 1
            print(f"PASS (expected compile failure): {source.relative_to(ROOT)}")

        for index, header in enumerate(PUBLIC_HEADERS):
            translation_unit = temp / f"header-{index}.cpp"
            relative = header.relative_to(ROOT / "src").as_posix()
            translation_unit.write_text(f"#include <{relative}>\nint main() {{ return 0; }}\n")
            executable = temp / f"header-{index}"
            if not run([CXX, *COMMON, str(translation_unit), "-o", str(executable)], True):
                print(f"FAIL: standalone header compile: {relative}")
                return 1
        print(f"PASS: standalone public headers ({len(PUBLIC_HEADERS)})")

    print(f"EDP-Event tests PASS with {CXX}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
