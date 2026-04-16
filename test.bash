#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
killall -SIGKILL clipsim

set -e

CLIPSIM_BIN="bin/clipsim"
TEST_DIR="/tmp/clipsim_full_test"
XDG_CACHE_HOME="$TEST_DIR/.cache"
./build.sh || exit 1

# Ensure clean state
rm -rf "$TEST_DIR"
mkdir -p "$XDG_CACHE_HOME"

export XDG_CACHE_HOME

ls /tmp/clipsim_full_test
echo "Starting daemon..."
$CLIPSIM_BIN --daemon &
DAEMON_PID=$!

cleanup () {
    echo "Cleaning up..."
    kill $DAEMON_PID 2>/dev/null
    rm -rf "$TEST_DIR"
    setsid -f clipsim -d
}
trap cleanup EXIT

sleep 2

timeout 2s $CLIPSIM_BIN --daemon
timeout_status=$?
if [ $timeout_status == 124 ]; then
    echo "FAIL: A second daemon was successfully started (it should have been blocked)."
    exit 1
fi

echo "Triggering clipboard changes..."
echo -n "first_test_string" | xclip -selection clipboard
sleep 0.5
echo -n "second_test_string" | xclip -selection clipboard
sleep 0.5
echo -n "third_test_string" | xclip -selection clipboard
sleep 0.5

echo "Testing --print..."
PRINT_OUT=$($CLIPSIM_BIN -p)
if ! echo "$PRINT_OUT" | grep -q "first_test_string"; then
    echo "FAIL: --print did not output expected history."
    exit 1
fi

echo "Testing --info..."
INFO_OUT=$($CLIPSIM_BIN -i 0)
if ! echo "$INFO_OUT" | grep -q "Length:"; then
    echo "FAIL: --info did not output the expected length metadata."
    exit 1
fi

echo "Testing --copy..."
$CLIPSIM_BIN -c 0
sleep 0.5
CLIP_DATA=$(xclip -o -selection clipboard)
if [ -z "$CLIP_DATA" ]; then
    echo "FAIL: --copy resulted in an empty clipboard."
    exit 1
fi

echo "Testing --save..."
$CLIPSIM_BIN -s
sleep 0.5
if [ ! -s "$XDG_CACHE_HOME/clipsim/history" ]; then
    echo "FAIL: --save failed, history file is missing or empty."
    exit 1
fi

echo "Testing --remove..."
$CLIPSIM_BIN -r 0
sleep 0.5
NEW_PRINT_OUT=$($CLIPSIM_BIN -p)

OLD_LINES=$(echo "$PRINT_OUT" | wc -l)
NEW_LINES=$(echo "$NEW_PRINT_OUT" | wc -l)
if [ "$NEW_LINES" -ge "$OLD_LINES" ]; then
    echo "FAIL: --remove did not decrease the number of history entries."
    exit 1
fi

echo "Testing --help..."
HELP_OUT=$($CLIPSIM_BIN -h)
if ! echo "$HELP_OUT" | grep -q "Available commands:"; then
    echo "FAIL: --help did not output the expected usage text."
    exit 1
fi

echo "--- All tests passed successfully! ---"
