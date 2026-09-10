#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
mode=${1:-release}
case "$mode" in
    release|debug) ;;
    *) echo "Usage: $0 [release|debug]" >&2; exit 2 ;;
esac
case "$(uname -s)" in
    Darwin) host=macos ;;
    *)      host=linux ;;
esac
cmake --preset "$host-$mode"
cmake --build --preset "$host-$mode" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-8}"
ctest --preset "$host-$mode" --output-on-failure
printf '\nRun: %s/build/%s-%s/stage/davesplorer\n' "$PWD" "$host" "$mode"
