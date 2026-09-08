#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
mode=${1:-release}
case "$mode" in
    release|debug) ;;
    *) echo "Usage: $0 [release|debug]" >&2; exit 2 ;;
esac
cmake --preset "linux-$mode"
cmake --build --preset "linux-$mode" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-8}"
ctest --preset "linux-$mode" --output-on-failure
printf '\nRun: %s/build/linux-%s/stage/davesplorer\n' "$PWD" "$mode"
