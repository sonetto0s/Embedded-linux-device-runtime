#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="${MINISHELL_BIN:-$ROOT_DIR/shell}"

TMP_ROOT="$(mktemp -d /tmp/minishell-stability.XXXXXX)"
FIFO="$TMP_ROOT/input.fifo"
LOG="$TMP_ROOT/output.log"
REDIRECT_FILE="$TMP_ROOT/redirect.txt"

SHELL_PID=""
FD_START=0
FD_END=0
RSS_START=0
RSS_END=0

cleanup()
{
    if [[ -n "$SHELL_PID" ]] && kill -0 "$SHELL_PID" 2>/dev/null
    then
        kill "$SHELL_PID" 2>/dev/null || true
        wait "$SHELL_PID" 2>/dev/null || true
    fi

    exec 3>&- 2>/dev/null || true
    rm -rf "$TMP_ROOT"
}

fail()
{
    echo "[FAIL] $*" >&2

    if [[ -f "$LOG" ]]
    then
        echo
        echo "========== MiniShell Last Output ==========" >&2
        tail -n 80 "$LOG" >&2 || true
        echo "===========================================" >&2
    fi

    exit 1
}

pass()
{
    echo "[PASS] $*"
}

send_command()
{
    printf '%s\n' "$1" >&3
}

count_fds()
{
    if [[ ! -d "/proc/$SHELL_PID/fd" ]]
    then
        echo 0
        return
    fi

    ls -1 "/proc/$SHELL_PID/fd" 2>/dev/null | wc -l
}

read_rss_kb()
{
    if [[ ! -r "/proc/$SHELL_PID/status" ]]
    then
        echo 0
        return
    fi

    awk '/^VmRSS:/ { print $2; exit }' "/proc/$SHELL_PID/status"
}

wait_for_shell()
{
    local retry

    for retry in $(seq 1 100)
    do
        if ! kill -0 "$SHELL_PID" 2>/dev/null
        then
            fail "MiniShell exited during startup"
        fi

        if [[ -d "/proc/$SHELL_PID/fd" ]]
        then
            return
        fi

        sleep 0.05
    done

    fail "MiniShell startup timeout"
}

wait_for_marker()
{
    local marker="$1"
    local retry

    for retry in $(seq 1 900)
    do
        if grep -Fq "$marker" "$LOG" 2>/dev/null
        then
            return
        fi

        if ! kill -0 "$SHELL_PID" 2>/dev/null
        then
            fail "MiniShell exited before stability workload completed"
        fi

        sleep 0.1
    done

    fail "stability workload timeout"
}

check_zombies()
{
    local zombies

    zombies="$(ps -o stat= --ppid "$SHELL_PID" 2>/dev/null | grep -c '^[[:space:]]*Z' || true)"

    if [[ "$zombies" -ne 0 ]]
    then
        fail "zombie processes detected: $zombies"
    fi

    pass "no zombie child processes"
}

check_log()
{
    if grep -Eqi 'segmentation fault|double free|corrupted|invalid pointer|assertion.*failed' "$LOG"
    then
        fail "fatal runtime error detected in output"
    fi

    pass "no fatal runtime errors in output"
}

if [[ ! -x "$BIN" ]]
then
    fail "MiniShell binary not found or not executable: $BIN"
fi

echo "=========================================="
echo " MiniShell ARM Runtime Stability Validation"
echo "=========================================="
echo
echo "Binary : $BIN"
echo "Temp   : $TMP_ROOT"
echo

mkfifo "$FIFO"

"$BIN" <"$FIFO" >"$LOG" 2>&1 &
SHELL_PID=$!

exec 3>"$FIFO"

wait_for_shell

sleep 0.2

FD_START="$(count_fds)"
RSS_START="$(read_rss_kb)"

echo "Initial PID : $SHELL_PID"
echo "Initial FD  : $FD_START"
echo "Initial RSS : ${RSS_START} KB"
echo

echo "[1/8] foreground command stress"

for i in $(seq 1 300)
do
    send_command "true"
done

echo "[2/8] pipeline stress"

for i in $(seq 1 100)
do
    send_command "printf abc | wc -c"
done

echo "[3/8] redirect stress"

for i in $(seq 1 100)
do
    send_command "echo stable > $REDIRECT_FILE"
    send_command "cat < $REDIRECT_FILE > /dev/null"
done

echo "[4/8] config reload stress"

for i in $(seq 1 100)
do
    send_command "reload"
done

echo "[5/8] working-directory independent reload"

send_command "cd /tmp"

for i in $(seq 1 50)
do
    send_command "reload"
done

echo "[6/8] background process stress"

for i in $(seq 1 200)
do
    send_command "true &"
done

send_command "sleep 2"

echo "[7/8] hardware information stress"

for i in $(seq 1 20)
do
    send_command "sysinfo"
    send_command "dtinfo"
    send_command "hwinfo"
    send_command "led list"
done

echo "[8/8] completion synchronization"

MARKER="__MINISHELL_ARM_STABILITY_DONE__"

send_command "echo $MARKER"

wait_for_marker "$MARKER"

sleep 1

if ! kill -0 "$SHELL_PID" 2>/dev/null
then
    fail "MiniShell is not alive after workload"
fi

pass "MiniShell remained alive during workload"

if [[ ! -f "$REDIRECT_FILE" ]]
then
    fail "redirect output file was not created"
fi

if [[ "$(cat "$REDIRECT_FILE")" != "stable" ]]
then
    fail "redirect output content is incorrect"
fi

pass "pipeline and redirect workload completed"

FD_END="$(count_fds)"
RSS_END="$(read_rss_kb)"

echo
echo "========== Resource Result =========="
echo "FD start  : $FD_START"
echo "FD end    : $FD_END"
echo "RSS start : ${RSS_START} KB"
echo "RSS end   : ${RSS_END} KB"
echo "====================================="
echo

if [[ "$FD_END" -ne "$FD_START" ]]
then
    fail "file descriptor count changed: $FD_START -> $FD_END"
fi

pass "file descriptor count returned to baseline"

RSS_GROWTH=$((RSS_END - RSS_START))

if [[ "$RSS_GROWTH" -lt 0 ]]
then
    RSS_GROWTH=0
fi

if [[ "$RSS_GROWTH" -gt 16384 ]]
then
    fail "RSS growth is too large: ${RSS_GROWTH} KB"
fi

pass "RSS growth is within limit: ${RSS_GROWTH} KB"

check_zombies
check_log

send_command "exit"

exec 3>&-

set +e
wait "$SHELL_PID"
SHELL_STATUS=$?
set -e

SHELL_PID=""

if [[ "$SHELL_STATUS" -ne 0 ]]
then
    fail "MiniShell exit status is $SHELL_STATUS"
fi

pass "MiniShell exited normally"

echo
echo "=========================================="
echo " ARM runtime stability validation passed"
echo "=========================================="
echo
echo "Commands:"
echo "  foreground : 300"
echo "  pipeline   : 100"
echo "  redirect   : 200"
echo "  reload     : 150"
echo "  background : 200"
echo "  hw groups  : 20"
echo
echo "Final FD  : $FD_END"
echo "Final RSS : ${RSS_END} KB"
