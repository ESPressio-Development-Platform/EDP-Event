#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PRODUCTION = ROOT / "src" / "event"
STABLE_PIOARDUINO = "https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def check_forbidden_production_primitives() -> None:
    forbidden = {
        "std::move(": "direct std::move instead of the EDP-Memory ownership abstraction",
        "::new (": "direct placement-new instead of EDP-Memory ObjectLifetime",
        ".~": "direct explicit destruction instead of EDP-Memory ObjectLifetime",
    }
    for path in sorted(PRODUCTION.glob("*.hpp")):
        text = path.read_text()
        for token, reason in forbidden.items():
            if token in text:
                fail(f"{path.relative_to(ROOT)} contains {reason}: {token}")



def check_strong_operation_results() -> None:
    forbidden = (
        (PRODUCTION / "Occurrence.hpp", "bool Claim("),
        (PRODUCTION / "Runtime.hpp", "bool TryDeliverOne("),
        (PRODUCTION / "Runtime.hpp", "bool TryDeliverOrdinal("),
    )
    for path, signature in forbidden:
        if signature in path.read_text():
            fail(f"{path.relative_to(ROOT)} reintroduced Boolean operation status {signature}")


def check_visibility_order_regressions() -> None:
    targets = (
        (ROOT / "src" / "event" / "Bootstrap.hpp", "class BootstrapImpl<"),
        (ROOT / "src" / "event" / "EventTypes.hpp", "class RemoteDispatchAttempt final"),
        (ROOT / "src" / "event" / "Occurrence.hpp", "class OccurrenceRecord final"),
        (ROOT / "tests" / "host" / "runtime.cpp", "class FakePool final"),
        (ROOT / "tests" / "host" / "concurrency.cpp", "class FakePool final"),
    )
    access = re.compile(r"\b(private|protected|public):")
    for path, marker in targets:
        text = path.read_text()
        start = text.find(marker)
        if start < 0:
            fail(f"cannot locate visibility-audited Type marker {marker!r} in {path.relative_to(ROOT)}")
        match = access.search(text, start)
        if match is None or match.group(1) != "private":
            fail(f"{path.relative_to(ROOT)} Type {marker!r} must begin with private visibility")

def check_remote_result_contract() -> None:
    text = (PRODUCTION / "EventTypes.hpp").read_text()
    if "ResultIfPresent()" not in text:
        fail("RemoteDispatchAttempt must expose presence-aware ResultIfPresent()")
    if re.search(r"\bResult\s*\(\s*\)\s*noexcept", text):
        fail("RemoteDispatchAttempt must not expose unconditional Result() access")


def check_document_reachability() -> None:
    readme = (ROOT / "README.MD").read_text()
    for path in sorted((ROOT / "docs").glob("*.MD")):
        relative = f"./docs/{path.name}"
        if relative not in readme:
            fail(f"root README does not link focused document {relative}")


def check_wiki_reference_files() -> None:
    for path in sorted(PRODUCTION.glob("*.hpp")):
        reference = ROOT / "wiki" / f"Reference-event-{path.stem}.md"
        if not reference.is_file():
            fail(f"missing Wiki reference page for {path.relative_to(ROOT)}")
    if not (ROOT / "wiki" / "Reference-ESPressio_Event.md").is_file():
        fail("missing Wiki reference page for src/ESPressio_Event.hpp")


def check_pioarduino_baseline() -> None:
    configs = sorted((ROOT / "demos").glob("*/PlatformIO_*/platformio.ini"))
    configs.append(ROOT / "examples" / "TemperatureMonitor" / "platformio.ini")
    for path in configs:
        text = path.read_text()
        if f"platform = {STABLE_PIOARDUINO}" not in text:
            fail(f"{path.relative_to(ROOT)} does not use current PIOArduino stable selector")
        if "-std=gnu++20" not in text:
            fail(f"{path.relative_to(ROOT)} does not explicitly enable C++20")


def check_stale_audit_identifiers() -> None:
    searchable = [ROOT / "README.MD", ROOT / "docs", ROOT / "wiki"]
    stale = ("RetentionValid", "55.3.311", "d41d8383706dc146fd91a0e1d3c59a915a4d493e")
    for entry in searchable:
        files = [entry] if entry.is_file() else list(entry.rglob("*"))
        for path in files:
            if not path.is_file():
                continue
            text = path.read_text(errors="ignore")
            for token in stale:
                if token in text:
                    fail(f"stale audit identifier {token!r} remains in {path.relative_to(ROOT)}")


def check_namespace_spacing() -> None:
    files = list((ROOT / "src").rglob("*.hpp")) + list((ROOT / "tests").rglob("*.hpp")) + list((ROOT / "tests").rglob("*.cpp"))
    declaration = re.compile(r"^\s*namespace\s+[A-Za-z_][\w:]*\s*\{\s*$")
    closing = re.compile(r"^\s*}\s*//\s*[A-Za-z_][\w:]*\s*$")
    for path in files:
        lines = path.read_text().splitlines()
        for index, line in enumerate(lines):
            if declaration.match(line) and index + 1 < len(lines) and lines[index + 1].strip():
                fail(f"{path.relative_to(ROOT)}:{index + 1} lacks blank line after namespace declaration")
            if closing.match(line) and index > 0 and lines[index - 1].strip():
                fail(f"{path.relative_to(ROOT)}:{index + 1} lacks blank line before namespace closure")


def main() -> int:
    check_forbidden_production_primitives()
    check_strong_operation_results()
    check_visibility_order_regressions()
    check_remote_result_contract()
    check_document_reachability()
    check_wiki_reference_files()
    check_pioarduino_baseline()
    check_stale_audit_identifiers()
    check_namespace_spacing()
    print("EDP-Event V2 policy regression checks: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
