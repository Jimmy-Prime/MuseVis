#!/usr/bin/env bash
set -euo pipefail

# ── Helpers ──────────────────────────────────────────────────────────────────

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
DIM='\033[0;90m'
RESET='\033[0m'

log()   { echo -e "${CYAN}[INFO]${RESET}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
err()   { echo -e "${RED}[ERROR]${RESET} $*"; }
ok()    { echo -e "${GREEN}[OK]${RESET}    $*"; }
debug() { echo -e "${DIM}[DEBUG] $*${RESET}"; }

SINK_NAME="musevis_sink"
MONITOR="${SINK_NAME}.monitor"
VERIFY_SECONDS=5

# ── Step 0: Pre-flight checks ───────────────────────────────────────────────

log "Checking prerequisites..."

for cmd in pactl parec sox; do
    if command -v "$cmd" &>/dev/null; then
        debug "Found: $(command -v "$cmd")"
    else
        err "'$cmd' not found. Install it first."
        exit 1
    fi
done

debug "PulseAudio server info:"
pactl info 2>&1 | grep -E "Server Name|Server Version|Default Sink|Default Source" | while read -r line; do
    debug "  $line"
done

# ── Step 1: Cleanup previous musevis modules ────────────────────────────────

log "Cleaning up any existing musevis modules..."

debug "Current modules before cleanup:"
pactl list short modules 2>&1 | while read -r line; do
    debug "  $line"
done

# Unload all loopback modules that reference our sink
LOOPBACK_IDS=$(pactl list short modules 2>/dev/null \
    | grep "module-loopback" \
    | while read -r id name args; do
        if echo "$args" | grep -q "$SINK_NAME"; then
            echo "$id"
        fi
    done) || true

if [ -n "$LOOPBACK_IDS" ]; then
    for id in $LOOPBACK_IDS; do
        log "Unloading musevis loopback module #$id"
        if pactl unload-module "$id" 2>&1; then
            ok "Unloaded loopback module #$id"
        else
            warn "Failed to unload loopback module #$id (may already be gone)"
        fi
    done
else
    debug "No existing musevis loopback modules found"
fi

# Unload all null-sink modules that match our sink name
NULLSINK_IDS=$(pactl list short modules 2>/dev/null \
    | grep "module-null-sink" \
    | while read -r id name args; do
        if echo "$args" | grep -q "sink_name=$SINK_NAME"; then
            echo "$id"
        fi
    done) || true

if [ -n "$NULLSINK_IDS" ]; then
    for id in $NULLSINK_IDS; do
        log "Unloading musevis null-sink module #$id"
        if pactl unload-module "$id" 2>&1; then
            ok "Unloaded null-sink module #$id"
        else
            warn "Failed to unload null-sink module #$id (may already be gone)"
        fi
    done
else
    debug "No existing musevis null-sink modules found"
fi

# Move any streams still pointing at musevis_sink back to the system default
DEFAULT_SINK=$(pactl info 2>/dev/null | grep "Default Sink:" | awk '{print $3}')
debug "Current default sink: ${DEFAULT_SINK:-<unknown>}"

ORPHAN_INPUTS=$(pactl list short sink-inputs 2>/dev/null \
    | grep "$SINK_NAME" \
    | awk '{print $1}') || true

if [ -n "$ORPHAN_INPUTS" ] && [ -n "$DEFAULT_SINK" ]; then
    for input_id in $ORPHAN_INPUTS; do
        log "Moving orphaned sink-input #$input_id to $DEFAULT_SINK"
        pactl move-sink-input "$input_id" "$DEFAULT_SINK" 2>&1 || true
    done
fi

debug "Modules after cleanup:"
pactl list short modules 2>&1 | while read -r line; do
    debug "  $line"
done

ok "Cleanup complete"
echo ""

# ── Step 2: Discover hardware sink ──────────────────────────────────────────

log "Discovering hardware sinks..."

debug "All sinks:"
pactl list short sinks 2>&1 | while read -r line; do
    debug "  $line"
done

# Pick the first non-musevis, non-null sink as the hardware sink
HW_SINK=""
while read -r _id name _module _sample _state; do
    if [[ "$name" != *"null"* && "$name" != *"musevis"* ]]; then
        HW_SINK="$name"
        break
    fi
done < <(pactl list short sinks 2>/dev/null)

if [ -z "$HW_SINK" ]; then
    err "No hardware sink found! Available sinks:"
    pactl list short sinks
    exit 1
fi

ok "Selected hardware sink: $HW_SINK"

# Show details about the hardware sink
debug "Hardware sink details:"
pactl list sinks 2>/dev/null | awk -v sink="$HW_SINK" '
    /^\s*Name:/ { found = ($2 == sink) }
    found && /^\s*(Name|Description|State|Sample Spec|Volume|Mute):/ { print "  " $0 }
    found && /^$/ { found = 0 }
'

echo ""

# ── Step 3: Create null-sink ────────────────────────────────────────────────

log "Creating null-sink '$SINK_NAME'..."

NULL_MODULE_ID=$(pactl load-module module-null-sink \
    sink_name="$SINK_NAME" \
    sink_properties=device.description="MuseVis_Capture" 2>&1)

if [ $? -ne 0 ] || [ -z "$NULL_MODULE_ID" ]; then
    err "Failed to create null-sink: $NULL_MODULE_ID"
    exit 1
fi

ok "Null-sink created (module #$NULL_MODULE_ID)"

# Verify it appeared
debug "Verifying null-sink exists in sink list..."
if pactl list short sinks 2>/dev/null | grep -q "$SINK_NAME"; then
    ok "Confirmed: $SINK_NAME appears in sink list"
else
    err "$SINK_NAME NOT found in sink list after creation!"
    debug "Current sinks:"
    pactl list short sinks 2>&1 | while read -r line; do debug "  $line"; done
    exit 1
fi

# Verify monitor source exists
debug "Verifying monitor source exists..."
if pactl list short sources 2>/dev/null | grep -q "$MONITOR"; then
    ok "Confirmed: $MONITOR appears in source list"
else
    err "$MONITOR NOT found in source list!"
    debug "Current sources:"
    pactl list short sources 2>&1 | while read -r line; do debug "  $line"; done
    exit 1
fi

debug "Null-sink details:"
pactl list sinks 2>/dev/null | awk -v sink="$SINK_NAME" '
    /^\s*Name:/ { found = ($2 == sink) }
    found && /^\s*(Name|Description|State|Sample Spec|Channel Map|Volume):/ { print "  " $0 }
    found && /^$/ { found = 0 }
'

echo ""

# ── Step 4: Create loopback ─────────────────────────────────────────────────

log "Creating loopback: $MONITOR → $HW_SINK (latency=20ms)..."

LOOPBACK_MODULE_ID=$(pactl load-module module-loopback \
    source="$MONITOR" \
    sink="$HW_SINK" \
    latency_msec=20 2>&1)

if [ $? -ne 0 ] || [ -z "$LOOPBACK_MODULE_ID" ]; then
    err "Failed to create loopback: $LOOPBACK_MODULE_ID"
    exit 1
fi

ok "Loopback created (module #$LOOPBACK_MODULE_ID)"

echo ""

# ── Step 5: Set default sink ────────────────────────────────────────────────

OLD_DEFAULT=$(pactl info 2>/dev/null | grep "Default Sink:" | awk '{print $3}')
debug "Previous default sink: $OLD_DEFAULT"

log "Setting default sink to $SINK_NAME..."
pactl set-default-sink "$SINK_NAME"

NEW_DEFAULT=$(pactl info 2>/dev/null | grep "Default Sink:" | awk '{print $3}')
if [ "$NEW_DEFAULT" = "$SINK_NAME" ]; then
    ok "Default sink is now: $NEW_DEFAULT"
else
    err "Default sink is '$NEW_DEFAULT', expected '$SINK_NAME'"
    exit 1
fi

echo ""

# ── Step 6: Move existing streams ───────────────────────────────────────────

log "Moving existing audio streams to $SINK_NAME..."

STREAM_COUNT=0
while read -r input_id _client sink_name _rest; do
    if [ "$sink_name" != "$SINK_NAME" ] 2>/dev/null; then
        # Get the application name for logging
        APP_NAME=$(pactl list sink-inputs 2>/dev/null | awk -v id="$input_id" '
            /^Sink Input #/ { current = ($3 == "#"id) ? 1 : 0 }
            current && /application.name/ { gsub(/"/, "", $3); print $3; exit }
        ')
        log "Moving sink-input #$input_id ($APP_NAME) → $SINK_NAME"
        if pactl move-sink-input "$input_id" "$SINK_NAME" 2>&1; then
            ok "Moved sink-input #$input_id"
            STREAM_COUNT=$((STREAM_COUNT + 1))
        else
            warn "Failed to move sink-input #$input_id"
        fi
    fi
done < <(pactl list short sink-inputs 2>/dev/null)

if [ "$STREAM_COUNT" -eq 0 ]; then
    debug "No existing streams needed moving"
else
    ok "Moved $STREAM_COUNT stream(s) to $SINK_NAME"
fi

echo ""

# ── Step 7: Full state dump ─────────────────────────────────────────────────

log "Current audio state:"
echo ""
debug "=== SINKS ==="
pactl list short sinks 2>&1 | while read -r line; do debug "  $line"; done
echo ""
debug "=== SOURCES ==="
pactl list short sources 2>&1 | while read -r line; do debug "  $line"; done
echo ""
debug "=== SINK INPUTS (active streams) ==="
pactl list short sink-inputs 2>&1 | while read -r line; do debug "  $line"; done
echo ""
debug "=== LOADED MODULES (musevis-related) ==="
pactl list short modules 2>&1 | grep -E "null-sink|loopback" | while read -r line; do debug "  $line"; done
echo ""

# ── Step 8: Verify with audio capture ───────────────────────────────────────

log "Starting ${VERIFY_SECONDS}s audio capture test from $MONITOR..."
warn "Play some music NOW if you haven't already!"
echo ""

# Capture audio for VERIFY_SECONDS seconds, then analyze
STATS=$(timeout "$VERIFY_SECONDS" parec -d "$MONITOR" --rate=44100 --channels=2 --format=s16le 2>/dev/null \
    | sox -t raw -r 44100 -e signed -b 16 -c 2 - -n stat 2>&1) || true

echo ""
log "sox stat output:"
echo "$STATS" | while read -r line; do
    debug "  $line"
done
echo ""

# Parse key values
SAMPLES=$(echo "$STATS" | grep "Samples read:" | awk '{print $3}')
RMS_AMP=$(echo "$STATS" | grep "RMS     amplitude:" | awk '{print $3}')
MAX_AMP=$(echo "$STATS" | grep "Maximum amplitude:" | awk '{print $3}')

debug "Parsed: samples=$SAMPLES  rms=$RMS_AMP  max=$MAX_AMP"

if [ -z "$SAMPLES" ] || [ "$SAMPLES" = "0" ]; then
    err "No samples captured! parec could not read from $MONITOR"
    err "Check: does the source exist? (pactl list short sources)"
    exit 1
fi

if [ "$RMS_AMP" = "0.000000" ] && [ "$MAX_AMP" = "0.000000" ]; then
    err "Captured $SAMPLES samples but audio is SILENT (all zeros)."
    err ""
    err "Possible causes:"
    err "  1. No music is playing"
    err "  2. Music app is still sending to the old sink, not $SINK_NAME"
    err "     → Check:  pactl list short sink-inputs"
    err "     → Fix:    restart your music player, or run:"
    err "               pactl move-sink-input <ID> $SINK_NAME"
    err "  3. Music app volume is muted"
    exit 1
fi

ok "Audio capture verified!"
ok "  Samples: $SAMPLES"
ok "  RMS amplitude: $RMS_AMP"
ok "  Max amplitude: $MAX_AMP"
echo ""
ok "Setup complete. You can now run:"
echo "    ./run-tui.sh $MONITOR"
echo "  or"
echo "    build/tui/musevis-tui $MONITOR"
