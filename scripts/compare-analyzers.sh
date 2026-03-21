#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "Usage: bash scripts/compare-analyzers.sh <binary> <pulse-source> [seconds]" >&2
    echo "Example: bash scripts/compare-analyzers.sh build/musevis-tui musevis_sink.monitor 10" >&2
    exit 1
fi

BINARY="$1"
SOURCE="$2"
DURATION="${3:-10}"

if ! command -v timeout >/dev/null 2>&1; then
    echo "'timeout' is required for this script." >&2
    exit 1
fi

run_capture() {
    local analyzer="$1"
    local output="$2"

    echo "Capturing ${analyzer} analyzer output -> ${output}"
    rm -f "$output"
    timeout "${DURATION}s" \
        env MUSEVIS_ANALYZER="$analyzer" MUSEVIS_BAND_DUMP="$output" \
        "$BINARY" "$SOURCE" >/dev/null 2>&1 || true

    if [ ! -f "$output" ]; then
        echo "No dump created for analyzer '${analyzer}'." >&2
        exit 1
    fi
}

run_capture fft fft.csv
run_capture filterbank filterbank.csv

echo ""
echo "Created:"
echo "  fft.csv"
echo "  filterbank.csv"
echo ""
echo "Compare the two dumps to check:"
echo "  - whether bass still suppresses mids/highs"
echo "  - whether silence decays close to zero"
echo "  - whether transients produce cleaner band motion"
