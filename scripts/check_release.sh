#!/usr/bin/env bash

set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

PASS_COUNT=0
CURRENT_STAGE="startup"

on_error()
{
    local line="$1"
    local code="$2"

    echo
    echo "================================================"
    echo " RELEASE CHECK FAILED"
    echo " Stage : $CURRENT_STAGE"
    echo " Line  : $line"
    echo " Code  : $code"
    echo "================================================"
}

trap 'on_error "$LINENO" "$?"' ERR

stage()
{
    CURRENT_STAGE="$1"

    echo
    echo "================================================"
    echo " $CURRENT_STAGE"
    echo "================================================"
}

pass()
{
    PASS_COUNT=$((PASS_COUNT + 1))
    echo "[PASS] $1"
}

require_file()
{
    if [ ! -f "$1" ]; then
        echo "Missing required file: $1"
        return 1
    fi
}

stage "[1/15] Repository sanity"

git rev-parse --is-inside-work-tree >/dev/null
git diff --check
git diff --cached --check

BRANCH="$(git branch --show-current)"

case "$BRANCH" in
    feature/v1.7-runtime-manager|main)
        ;;
    *)
        echo "Unexpected branch: $BRANCH"
        exit 1
        ;;
esac

if ! grep -Eq 'VERSION[[:space:]]+1\.7([[:space:]]|$)' CMakeLists.txt; then
    echo "CMake project version is not V1.7"
    exit 1
fi

bash -n tests/stability/arm_runtime_stability.sh

pass "Repository sanity"

stage "[2/15] Runtime architecture"

if grep -R "process_manager" -n include src tests Makefile CMakeLists.txt; then
    echo "Old process_manager reference found"
    exit 1
fi

if grep -E "cmd_runtime|shell_context|builtin" \
    src/runtime_monitor.c \
    src/runtime_snapshot.c \
    src/network_monitor.c \
    src/thermal_monitor.c \
    src/process_monitor.c; then

    echo "Runtime Core depends on upper Shell/Command layer"
    exit 1
fi

require_file include/runtime_monitor.h
require_file include/process_monitor.h
require_file include/network_monitor.h
require_file include/thermal_monitor.h
require_file include/runtime_snapshot.h

require_file src/runtime_monitor.c
require_file src/process_monitor.c
require_file src/network_monitor.c
require_file src/thermal_monitor.c
require_file src/runtime_snapshot.c

pass "Runtime architecture"

stage "[3/15] Clean native build"

make clean
make -j"$(nproc)"

require_file shell

pass "Native build"

stage "[4/15] Unit and integration tests"

make check

pass "Unit and integration tests"

stage "[5/15] Strict build"

make clean
make strict

pass "Strict build"

stage "[6/15] Sanitizers"

make clean
make asan

pass "ASan and UBSan"

stage "[7/15] Static analysis"

if command -v cppcheck >/dev/null 2>&1; then
    make cppcheck
    pass "Cppcheck"
else
    echo "[SKIP] cppcheck is not installed"
fi

stage "[8/15] Runtime smoke test"

make clean
make -j"$(nproc)"

SMOKE_OUTPUT="$(printf 'monitor\npsinfo 1\nexit\n' | ./shell 2>&1)"

printf '%s\n' "$SMOKE_OUTPUT"

printf '%s\n' "$SMOKE_OUTPUT" | grep -q "Runtime Monitor"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "CPU Usage"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "Memory Usage"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "Temperature"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "Process Count"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "Network"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "RX Bytes"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "TX Bytes"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "Uptime"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "Process Information"
printf '%s\n' "$SMOKE_OUTPUT" | grep -q "Threads"

pass "Runtime smoke test"

stage "[9/15] Runtime error-path smoke test"

ERROR_OUTPUT="$(
    printf 'monitor invalid\npsinfo abc\npsinfo -1\npsinfo 99999999\ntrue\nexit\n' |
        ./shell 2>&1
)"

printf '%s\n' "$ERROR_OUTPUT"

printf '%s\n' "$ERROR_OUTPUT" | grep -q "usage: monitor"
printf '%s\n' "$ERROR_OUTPUT" | grep -q "psinfo: invalid pid: abc"
printf '%s\n' "$ERROR_OUTPUT" | grep -q "psinfo: invalid pid: -1"
printf '%s\n' "$ERROR_OUTPUT" | grep -q "psinfo: unable to read process 99999999"

pass "Runtime error paths"

stage "[10/15] CMake Debug"

rm -rf build/cmake-debug

cmake -S . -B build/cmake-debug \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DMINISHELL_WARNINGS_AS_ERRORS=ON

cmake --build build/cmake-debug -j"$(nproc)"

(
    cd build/cmake-debug

    TEST_COUNT="$(ctest -N | awk '/Test #[0-9]+:/ { count++ } END { print count + 0 }')"

    if [ "$TEST_COUNT" -ne 2 ]; then
        echo "Expected 2 CTest tests, got $TEST_COUNT"
        exit 1
    fi

    ctest --output-on-failure
)

pass "CMake Debug"

stage "[11/15] CMake Release"

rm -rf build/cmake-release

cmake -S . -B build/cmake-release \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON \
    -DMINISHELL_WARNINGS_AS_ERRORS=ON

cmake --build build/cmake-release -j"$(nproc)"

(
    cd build/cmake-release

    TEST_COUNT="$(ctest -N | awk '/Test #[0-9]+:/ { count++ } END { print count + 0 }')"

    if [ "$TEST_COUNT" -ne 2 ]; then
        echo "Expected 2 CTest tests, got $TEST_COUNT"
        exit 1
    fi

    ctest --output-on-failure
)

pass "CMake Release"

stage "[12/15] Orange Pi ARM64 validation"

HOST_ARCH="$(uname -m)"

echo "Host architecture: $HOST_ARCH"

if [ "$HOST_ARCH" != "aarch64" ] && [ "$HOST_ARCH" != "arm64" ]; then
    echo "Release validation must run on the Orange Pi ARM64 host"
    exit 1
fi

make clean
make -j"$(nproc)"

if command -v file >/dev/null 2>&1; then
    file shell
    file shell | grep -Eq "ARM aarch64|ARM64|aarch64"
fi

pass "Native ARM64 runtime"

stage "[13/15] ARM runtime stability"

chmod +x tests/stability/arm_runtime_stability.sh
./tests/stability/arm_runtime_stability.sh

pass "ARM runtime stability"

stage "[14/15] Package and install validation"

make package

require_file dist/native/bin/minishell
require_file dist/native/config/config.conf

PACKAGE_OUTPUT="$(
    printf 'monitor\nexit\n' |
        ./dist/native/bin/minishell 2>&1
)"

printf '%s\n' "$PACKAGE_OUTPUT" | grep -q "Runtime Monitor"

INSTALL_ROOT="$(mktemp -d /tmp/minishell-install-XXXXXX)"

cleanup_install()
{
    rm -rf "$INSTALL_ROOT"
}

trap cleanup_install EXIT

make install DESTDIR="$INSTALL_ROOT"

require_file "$INSTALL_ROOT/opt/minishell/bin/minishell"
require_file "$INSTALL_ROOT/opt/minishell/config/config.conf"

INSTALL_OUTPUT="$(
    printf 'monitor\nexit\n' |
        "$INSTALL_ROOT/opt/minishell/bin/minishell" 2>&1
)"

printf '%s\n' "$INSTALL_OUTPUT" | grep -q "Runtime Monitor"

make uninstall DESTDIR="$INSTALL_ROOT"

if [ -e "$INSTALL_ROOT/opt/minishell/bin/minishell" ]; then
    echo "Installed binary still exists after uninstall"
    exit 1
fi

cleanup_install
trap - EXIT

pass "Package and installation"

stage "[15/15] Optional Valgrind and final repository check"

if command -v valgrind >/dev/null 2>&1; then
    make clean
    make -j"$(nproc)"
    make valgrind
    pass "Valgrind"
else
    echo "[SKIP] valgrind is not installed"
fi

git diff --check
git diff --cached --check

echo
git status --short

echo
git diff --stat

echo
echo "================================================"
echo " V1.7 RELEASE GATE PASSED"
echo " Checks passed: $PASS_COUNT"
echo " Host: $(uname -m)"
echo " Branch: $(git branch --show-current)"
echo "================================================"
