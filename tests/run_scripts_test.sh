#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

LOG_FILE="$TMP_DIR/calls.log"
export LOG_FILE

cat > "$TMP_DIR/build-tui.sh" <<EOF
#!/usr/bin/env bash
printf '%s\n' "build-tui \$*" >> "$LOG_FILE"
mkdir -p "$ROOT_DIR/build/tui"
cat > "$ROOT_DIR/build/tui/musevis-tui" <<'INNER'
#!/usr/bin/env bash
printf '%s\n' "musevis-tui \$*" >> "\$LOG_FILE"
INNER
chmod +x "$ROOT_DIR/build/tui/musevis-tui"
EOF
chmod +x "$TMP_DIR/build-tui.sh"

cat > "$TMP_DIR/build-led.sh" <<EOF
#!/usr/bin/env bash
printf '%s\n' "build-led \$*" >> "$LOG_FILE"
mkdir -p "$ROOT_DIR/build/led"
cat > "$ROOT_DIR/build/led/musevis" <<'INNER'
#!/usr/bin/env bash
printf '%s\n' "musevis \$*" >> "\$LOG_FILE"
INNER
chmod +x "$ROOT_DIR/build/led/musevis"
EOF
chmod +x "$TMP_DIR/build-led.sh"

run_tui_and_assert() {
    local expected_build="$1"
    local expected_binary="$2"
    shift 2

    : > "$LOG_FILE"
    (
        cd "$ROOT_DIR"
        BUILD_TUI_SCRIPT="$TMP_DIR/build-tui.sh" "./run-tui.sh" "$@" >/dev/null
    )

    local calls=()
    while IFS= read -r line; do
        calls+=("$line")
    done < "$LOG_FILE"

    if [ "${#calls[@]}" -ne 2 ]; then
        echo "expected 2 calls for run-tui.sh, got ${#calls[@]}" >&2
        exit 1
    fi

    if [ "${calls[0]}" != "$expected_build" ]; then
        echo "unexpected build call for run-tui.sh" >&2
        echo "got: ${calls[0]}" >&2
        echo "exp: $expected_build" >&2
        exit 1
    fi

    if [ "${calls[1]}" != "$expected_binary" ]; then
        echo "unexpected binary call for run-tui.sh" >&2
        echo "got: ${calls[1]}" >&2
        echo "exp: $expected_binary" >&2
        exit 1
    fi
}

run_led_and_assert() {
    local expected_build="$1"
    local expected_binary="$2"
    shift 2

    : > "$LOG_FILE"
    (
        cd "$ROOT_DIR"
        BUILD_LED_SCRIPT="$TMP_DIR/build-led.sh" "./run-led.sh" "$@" >/dev/null
    )

    local calls=()
    while IFS= read -r line; do
        calls+=("$line")
    done < "$LOG_FILE"

    if [ "${#calls[@]}" -ne 2 ]; then
        echo "expected 2 calls for run-led.sh, got ${#calls[@]}" >&2
        exit 1
    fi

    if [ "${calls[0]}" != "$expected_build" ]; then
        echo "unexpected build call for run-led.sh" >&2
        echo "got: ${calls[0]}" >&2
        echo "exp: $expected_build" >&2
        exit 1
    fi

    if [ "${calls[1]}" != "$expected_binary" ]; then
        echo "unexpected binary call for run-led.sh" >&2
        echo "got: ${calls[1]}" >&2
        echo "exp: $expected_binary" >&2
        exit 1
    fi
}

run_tui_and_assert \
    "build-tui " \
    "musevis-tui musevis_sink.monitor" \
    "musevis_sink.monitor"

run_led_and_assert \
    "build-led " \
    "musevis musevis_sink.monitor" \
    "musevis_sink.monitor"

rm -rf "$ROOT_DIR/build/tui" "$ROOT_DIR/build/led"

echo "run script tests passed"
