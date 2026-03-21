#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY_PATH="${BUILD_DIR:-build/led}/musevis"
BUILD_SCRIPT="${BUILD_LED_SCRIPT:-$ROOT_DIR/build-led.sh}"

"$BUILD_SCRIPT"
exec "$ROOT_DIR/$BINARY_PATH" "$@"
