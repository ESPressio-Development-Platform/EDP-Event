#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="$(cd "${ROOT}/.." && pwd)"
PROJECT="examples/TemperatureMonitor"

required=(
    EDP-Event EDP-System EDP-Primitives EDP-Memory EDP-Threading EDP-Clock
    EDP-BoundedTopology EDP-BoundedTypes EDP-Platform EDP-Platform-Portable
    EDP-Platform-FreeRTOS EDP-Platform-ESP-IDF
)

for repository in "${required[@]}"; do
    if [[ ! -d "${WORKSPACE}/${repository}/src" ]]; then
        echo "Missing sibling repository: ${repository}" >&2
        exit 2
    fi
done

source_conf="${ROOT}/${PROJECT}/platformio.ini"
temp_conf="$(mktemp "${ROOT}/${PROJECT}/platformio.local.XXXXXX.ini")"
trap 'rm -f "${temp_conf}"' EXIT

python3 - "${source_conf}" "${temp_conf}" "${WORKSPACE}" <<'PY'
from pathlib import Path
import sys

source, target, workspace = map(Path, sys.argv[1:])
lines = source.read_text().splitlines()
output = []
skipping_dependencies = False

for line in lines:
    if line == "lib_deps =":
        skipping_dependencies = True
        continue
    if skipping_dependencies:
        if line.startswith("["):
            skipping_dependencies = False
        else:
            continue
    output.append(line)

repositories = [
    "EDP-Event", "EDP-System", "EDP-Primitives", "EDP-Memory", "EDP-Threading",
    "EDP-Clock", "EDP-BoundedTopology", "EDP-BoundedTypes", "EDP-Platform",
    "EDP-Platform-Portable", "EDP-Platform-FreeRTOS", "EDP-Platform-ESP-IDF",
]
expanded = []
for line in output:
    expanded.append(line)
    if line == "build_flags =":
        expanded.extend(
            f"    -I{workspace / repository / 'src'}"
            for repository in repositories
        )

target.write_text("\n".join(expanded) + "\n")
PY

rm -rf "${ROOT}/${PROJECT}/.pio"
pio run -d "${ROOT}/${PROJECT}" --project-conf "${temp_conf}"
echo "EDP-Event TemperatureMonitor example build: PASS"
