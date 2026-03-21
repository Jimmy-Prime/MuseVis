#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-build/tui}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake is required but was not found in PATH." >&2
    exit 1
fi

if [ -n "${BUILD_JOBS:-}" ]; then
    JOBS="$BUILD_JOBS"
elif command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
elif command -v sysctl >/dev/null 2>&1; then
    JOBS="$(sysctl -n hw.ncpu)"
else
    JOBS=4
fi

cd "$ROOT_DIR"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" --target musevis-tui -j"$JOBS"
