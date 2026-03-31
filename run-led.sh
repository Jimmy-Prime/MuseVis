#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY_PATH="${BUILD_DIR:-build/led}/musevis"
BUILD_SCRIPT="${BUILD_LED_SCRIPT:-$ROOT_DIR/build-led.sh}"

"$BUILD_SCRIPT"
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
exec sudo -E "$ROOT_DIR/$BINARY_PATH" "$@"
