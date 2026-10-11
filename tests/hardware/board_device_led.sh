#!/usr/bin/env bash

set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="${1:-$ROOT_DIR/build/strict/minishell}"
LED_DIR="/sys/class/leds/blue_led"
TMP_ROOT=""
ORIGINAL_TRIGGER=""
ORIGINAL_BRIGHTNESS=""
RESTORE_LED=0
TEST_COMPLETE=0

fail()
{
    echo "[FAIL] $*" >&2
    exit 1
}

pass()
{
    echo "[PASS] $*"
}

read_trigger()
{
    local text
    local pattern='\[([^]]+)\]'

    text="$(cat "$LED_DIR/trigger")" || return 1

    if [[ ! "$text" =~ $pattern ]]
    then
        return 1
    fi

    printf '%s\n' "${BASH_REMATCH[1]}"
}

cleanup()
{
    local result="$?"
    local restore_failed=0
    local actual_trigger
    local actual_brightness

    trap - EXIT ERR INT TERM
    set +e

    if [[ "$RESTORE_LED" -eq 1 ]]
    then
        if ! printf '%s\n' none > "$LED_DIR/trigger"
        then
            echo "[FAIL] Failed restore LED manual mode" >&2
            restore_failed=1
        fi

        if ! printf '%s\n' "$ORIGINAL_BRIGHTNESS" > "$LED_DIR/brightness"
        then
            echo "[FAIL] Failed restore LED brightness" >&2
            restore_failed=1
        fi

        if ! printf '%s\n' "$ORIGINAL_TRIGGER" > "$LED_DIR/trigger"
        then
            echo "[FAIL] Failed restore LED trigger" >&2
            restore_failed=1
        fi

        actual_trigger="$(read_trigger)"

        if [[ "$actual_trigger" != "$ORIGINAL_TRIGGER" ]]
        then
            echo "[FAIL] Restored LED trigger does not match" >&2
            restore_failed=1
        fi

        if [[ "$ORIGINAL_TRIGGER" == none ]]
        then
            actual_brightness="$(cat "$LED_DIR/brightness")"

            if [[ "$actual_brightness" != "$ORIGINAL_BRIGHTNESS" ]]
            then
                echo "[FAIL] Restored LED brightness does not match" >&2
                restore_failed=1
            fi
        fi
    fi

    if [[ -n "$TMP_ROOT" ]]
    then
        if ! rm -rf -- "$TMP_ROOT"
        then
            echo "[FAIL] Failed remove temporary test files: $TMP_ROOT" >&2
            restore_failed=1
        fi
    fi

    if [[ "$restore_failed" -ne 0 ]]
    then
        result=1
    fi

    if [[ "$result" -eq 0 && "$TEST_COMPLETE" -eq 1 ]]
    then
        pass "Original blue_led trigger and brightness restored"
        pass "Board Device LED validation completed"
    fi

    exit "$result"
}

trap cleanup EXIT
trap 'echo "[FAIL] Line $LINENO returned status $?" >&2' ERR
trap 'exit 130' INT
trap 'exit 143' TERM

run_commands()
{
    local stage="$1"
    local expected="$2"
    shift 2

    printf '%s\n' "$@" exit > "$TMP_ROOT/input.txt"

    if ! timeout --signal=TERM --kill-after=2s 15s \
        env MINISHELL_CONFIG="$TMP_ROOT/config.conf" \
            MINISHELL_DEVICES_CONFIG="$TMP_ROOT/devices.conf" \
            "$BIN" < "$TMP_ROOT/input.txt" \
            > "$TMP_ROOT/output.log" 2> "$TMP_ROOT/error.log"
    then
        cat "$TMP_ROOT/output.log" "$TMP_ROOT/error.log" >&2
        fail "$stage: MiniShell failed or timed out"
    fi

    sed 's/>>BoardLedReview //g; /^>>MiniShell /d; /^$/d' \
        "$TMP_ROOT/output.log" | tr -d '\r' > "$TMP_ROOT/actual.txt"
    printf '%s\n' "$expected" > "$TMP_ROOT/expected.txt"

    if ! diff -u "$TMP_ROOT/expected.txt" "$TMP_ROOT/actual.txt"
    then
        cat "$TMP_ROOT/error.log" >&2
        fail "$stage: unexpected command output or status"
    fi

    if grep -Eq '\[ERROR\]|device:|config:' "$TMP_ROOT/error.log"
    then
        cat "$TMP_ROOT/error.log" >&2
        fail "$stage: command error detected"
    fi
}

if [[ "$#" -gt 1 ]]
then
    fail "Usage: $0 [minishell_binary]"
fi

if [[ "$EUID" -ne 0 ]]
then
    fail "Run this test as root to write blue_led sysfs attributes"
fi

if [[ ! -x "$BIN" ]]
then
    fail "MiniShell binary is not executable: $BIN"
fi

for command in timeout env cat sed tr diff grep mktemp rm
do
    command -v "$command" >/dev/null || fail "Missing command: $command"
done

for attribute in brightness max_brightness trigger
do
    if [[ ! -r "$LED_DIR/$attribute" ]]
    then
        fail "Missing readable LED attribute: $LED_DIR/$attribute"
    fi
done

if [[ ! -w "$LED_DIR/brightness" || ! -w "$LED_DIR/trigger" ]]
then
    fail "LED brightness and trigger must be writable"
fi

if ! grep -Eq '(^|[[:space:]])\[?heartbeat\]?([[:space:]]|$)' "$LED_DIR/trigger"
then
    fail "blue_led does not provide the heartbeat trigger"
fi

ORIGINAL_TRIGGER="$(read_trigger)"
ORIGINAL_BRIGHTNESS="$(cat "$LED_DIR/brightness")"
MAX_BRIGHTNESS="$(cat "$LED_DIR/max_brightness")"

if [[ ! "$ORIGINAL_BRIGHTNESS" =~ ^[0-9]+$ ||
      ! "$MAX_BRIGHTNESS" =~ ^[0-9]+$ || "$MAX_BRIGHTNESS" -le 0 ]]
then
    fail "Invalid LED brightness attributes"
fi

TMP_ROOT="$(mktemp -d /tmp/minishell-board-led.XXXXXX)"

cat > "$TMP_ROOT/config.conf" <<'EOF'
prompts=BoardLedReview
max_job=64
debug=0
EOF

cat > "$TMP_ROOT/devices.conf" <<'EOF'
[review_led]
type=led
led_name=blue_led
enabled=1
EOF

RESTORE_LED=1

run_commands "LED on" $'review_led: open\n0\nreview_led: on\n0\nreview_led: closed\n0' \
    'device open review_led' status \
    'device led review_led on' status \
    'device close review_led' status

[[ "$(cat "$LED_DIR/brightness")" == "$MAX_BRIGHTNESS" ]] || fail "LED on brightness differs from max_brightness"
[[ "$(read_trigger)" == none ]] || fail "LED on did not select the none trigger"
pass "Device LED on: brightness=max_brightness, trigger=none"

run_commands "LED off" $'review_led: open\n0\nreview_led: off\n0\nreview_led: closed\n0' \
    'device open review_led' status \
    'device led review_led off' status \
    'device close review_led' status

[[ "$(cat "$LED_DIR/brightness")" == 0 ]] || fail "LED off brightness is not zero"
[[ "$(read_trigger)" == none ]] || fail "LED off did not select the none trigger"
pass "Device LED off: brightness=0, trigger=none"

run_commands "LED heartbeat" $'review_led: open\n0\nreview_led: trigger=heartbeat\n0\nreview_led: closed\n0' \
    'device open review_led' status \
    'device led review_led trigger heartbeat' status \
    'device close review_led' status

[[ "$(read_trigger)" == heartbeat ]] || fail "LED heartbeat trigger was not selected"
pass "Device LED trigger: heartbeat readback matched"

for ((cycle = 1; cycle <= 20; cycle++))
do
    run_commands "LED cycle $cycle" \
        $'review_led: open\n0\nreview_led: on\n0\nreview_led: off\n0\nreview_led: closed\n0' \
        'device open review_led' status \
        'device led review_led on' status \
        'device led review_led off' status \
        'device close review_led' status

    [[ "$(cat "$LED_DIR/brightness")" == 0 ]] || fail "LED cycle $cycle ended with nonzero brightness"
    [[ "$(read_trigger)" == none ]] || fail "LED cycle $cycle ended with an active trigger"
done

pass "20 Device open/on/off/close cycles: every output, status and final sysfs state matched"
TEST_COMPLETE=1
