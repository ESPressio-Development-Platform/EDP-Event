#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="$(cd "${ROOT}/.." && pwd)"
required=(EDP-Event EDP-System EDP-Primitives EDP-Serialisation EDP-Localisation EDP-Memory EDP-Threading EDP-Clock EDP-BoundedTopology EDP-BoundedTypes EDP-Platform EDP-Platform-Portable EDP-Platform-FreeRTOS EDP-Platform-ESP-IDF)
for repository in "${required[@]}"; do
    [[ -d "${WORKSPACE}/${repository}/src" ]] || { echo "Missing sibling repository: ${repository}" >&2; exit 2; }
done
build_project() {
    local project="$1"
    local source_conf="${ROOT}/${project}/platformio.ini"
    local temp_conf
    temp_conf="$(mktemp "${ROOT}/${project}/platformio.local.XXXXXX.ini")"
    python3 - "${source_conf}" "${temp_conf}" "${WORKSPACE}" <<'PY'
from pathlib import Path
import sys
source,target,workspace=map(Path,sys.argv[1:])
lines=source.read_text().splitlines(); output=[]; skipping=False
for line in lines:
    if line == 'lib_deps =': skipping=True; continue
    if skipping:
        if line.startswith('['): skipping=False
        else: continue
    output.append(line)
repos=['EDP-Event','EDP-System','EDP-Primitives','EDP-Serialisation','EDP-Localisation','EDP-Memory','EDP-Threading','EDP-Clock','EDP-BoundedTopology','EDP-BoundedTypes','EDP-Platform','EDP-Platform-Portable','EDP-Platform-FreeRTOS','EDP-Platform-ESP-IDF']
expanded=[]
for line in output:
    expanded.append(line)
    if line == 'build_flags =':
        expanded.extend(f'    -I{workspace / repo / "src"}' for repo in repos)
target.write_text('\n'.join(expanded)+'\n')
PY
    rm -rf "${ROOT}/${project}/.pio"
    pio run -d "${ROOT}/${project}" --project-conf "${temp_conf}"
    rm -f "${temp_conf}"
}
for demo in local-broadcast newest-only-retention transport-boundary; do
    echo "EDP-Event demo build: ${demo} / Arduino"
    build_project "demos/${demo}/PlatformIO_Arduino"
    echo "EDP-Event demo build: ${demo} / ESP-IDF"
    build_project "demos/${demo}/PlatformIO_ESP-IDF"
done
echo "EDP-Event demo builds: PASS"
