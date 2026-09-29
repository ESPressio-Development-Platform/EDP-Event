#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKSPACE="$(cd "${ROOT}/.." && pwd)"
ARDUINO_CLI="${ARDUINO_CLI:-arduino-cli}"
FQBN="${EDP_EVENT_ARDUINO_FQBN:-esp32:esp32:esp32}"

required=(
    EDP-Event EDP-System EDP-Primitives EDP-Memory EDP-Threading EDP-Clock
    EDP-BoundedTopology EDP-BoundedTypes EDP-Platform EDP-Platform-Portable
    EDP-Platform-FreeRTOS EDP-Platform-ESP-IDF
)

command -v "${ARDUINO_CLI}" >/dev/null 2>&1 || {
    echo "arduino-cli is required for Arduino IDE validation" >&2
    exit 2
}

library_args=()
cpp_flags="-std=gnu++20"
for repository in "${required[@]}"; do
    [[ -f "${WORKSPACE}/${repository}/library.properties" ]] || {
        echo "Missing Arduino library metadata: ${repository}" >&2
        exit 2
    }
    library_args+=(--library "${WORKSPACE}/${repository}")
    cpp_flags+=" -I${WORKSPACE}/${repository}/src"
done

for demo in local-broadcast newest-only-retention transport-boundary; do
    sketch="${ROOT}/demos/${demo}/Arduino_IDE"
    build="${sketch}/.arduino-build"
    rm -rf "${build}"

    echo "EDP-Event Arduino IDE build: ${demo}"
    "${ARDUINO_CLI}" compile \
        --fqbn "${FQBN}" \
        "${library_args[@]}" \
        --build-path "${build}" \
        --build-property "compiler.cpp.extra_flags=${cpp_flags}" \
        "${sketch}"

done

echo "EDP-Event Arduino IDE builds: PASS"
