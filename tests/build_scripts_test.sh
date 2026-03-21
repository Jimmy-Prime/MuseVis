#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

LOG_FILE="$TMP_DIR/cmake.log"

cat > "$TMP_DIR/cmake" <<EOF
#!/usr/bin/env bash
printf '%s\n' "\$*" >> "$LOG_FILE"
exit 0
EOF
chmod +x "$TMP_DIR/cmake"

PATH="$TMP_DIR:$PATH"

run_and_assert() {
    local script="$1"
    local expected_configure="$2"
    local expected_build="$3"

    : > "$LOG_FILE"
    (cd "$ROOT_DIR" && BUILD_JOBS=7 "./$script" >/dev/null)

    local calls=()
    while IFS= read -r line; do
        calls+=("$line")
    done < "$LOG_FILE"

    if [ "${#calls[@]}" -ne 2 ]; then
        echo "expected 2 cmake calls for $script, got ${#calls[@]}" >&2
        exit 1
    fi

    if [ "${calls[0]}" != "$expected_configure" ]; then
        echo "unexpected configure call for $script" >&2
        echo "got: ${calls[0]}" >&2
        echo "exp: $expected_configure" >&2
        exit 1
    fi

    if [ "${calls[1]}" != "$expected_build" ]; then
        echo "unexpected build call for $script" >&2
        echo "got: ${calls[1]}" >&2
        echo "exp: $expected_build" >&2
        exit 1
    fi
}

run_and_assert \
    "build-tui.sh" \
    "-S . -B build/tui -DCMAKE_BUILD_TYPE=Release" \
    "--build build/tui --target musevis-tui -j7"

run_and_assert \
    "build-led.sh" \
    "-S . -B build/led -DCMAKE_BUILD_TYPE=Release" \
    "--build build/led --target musevis -j7"

echo "build script tests passed"
