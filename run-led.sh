#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY_PATH="${BUILD_DIR:-build/led}/musevis"
BUILD_SCRIPT="${BUILD_LED_SCRIPT:-$ROOT_DIR/build-led.sh}"

"$BUILD_SCRIPT"
PULSE_SERVER="${PULSE_SERVER:-unix:/run/user/$(id -u)/pulse/native}"
exec sudo PULSE_SERVER="$PULSE_SERVER" "$ROOT_DIR/$BINARY_PATH" "$@"
