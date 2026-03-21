#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY_PATH="${BUILD_DIR:-build/tui}/musevis-tui"
BUILD_SCRIPT="${BUILD_TUI_SCRIPT:-$ROOT_DIR/build-tui.sh}"

"$BUILD_SCRIPT"
exec "$ROOT_DIR/$BINARY_PATH" "$@"
