#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

CLIPSIM_BIN="bin/clipsim_debug"
TEST_DIR="/tmp/clipsim_full_test"
XDG_CACHE_HOME="$TEST_DIR/.cache"
./build.sh debug || exit 1
# killall clipsim

# Ensure clean state
rm -rf "$TEST_DIR"
mkdir -p "$XDG_CACHE_HOME"

export XDG_CACHE_HOME

echo "--- Starting Clipsim Integration Tests ---"

# 1. Launch Daemon
echo "Starting daemon..."
$CLIPSIM_BIN --daemon &
DAEMON_PID=$!

# Ensure the daemon is killed and cleanup occurs on exit
trap 'echo "Cleaning up..."; kill $DAEMON_PID 2>/dev/null; rm -rf "$TEST_DIR"' EXIT

# Give the daemon a moment to initialize X11 connections and FIFOs
sleep 1

# Verify duplicate daemon prevention
echo "Testing duplicate daemon prevention..."
$CLIPSIM_BIN --daemon &
pid_other=$?
if wait $pid_other; then
    echo "FAIL: A second daemon was successfully started (it should have been blocked)."
    exit 1
fi

# 2. Test Clipboard Watcher
echo "Triggering clipboard changes..."
echo -n "first_test_string" | xclip -selection clipboard
sleep 0.5
echo -n "second_test_string" | xclip -selection clipboard
sleep 0.5
echo -n "third_test_string" | xclip -selection clipboard
sleep 0.5

# 3. Test --print (-p)
echo "Testing --print..."
PRINT_OUT=$($CLIPSIM_BIN -p)
if ! echo "$PRINT_OUT" | grep -q "first_test_string"; then
    echo "FAIL: --print did not output expected history."
    exit 1
fi

# 4. Test --info (-i)
# "third_test_string" is the most recent, so it should be at index 2 (if indices are 0, 1, 2)
# Since history_length would be 3, the top element depends on your ring buffer ordering.
# We will just grep the output to make sure it gives the detailed info view.
echo "Testing --info..."
INFO_OUT=$($CLIPSIM_BIN -i 0)
if ! echo "$INFO_OUT" | grep -q "Length:"; then
    echo "FAIL: --info did not output the expected length metadata."
    exit 1
fi

# 5. Test --copy (-c)
echo "Testing --copy..."
# Recover the very first item we copied (which should be pushed back in the ring)
$CLIPSIM_BIN -c 0
sleep 0.5
CLIP_DATA=$(xclip -o -selection clipboard)
if [ -z "$CLIP_DATA" ]; then
    echo "FAIL: --copy resulted in an empty clipboard."
    exit 1
fi

# 6. Test --save (-s)
echo "Testing --save..."
$CLIPSIM_BIN -s
sleep 0.5
if [ ! -s "$XDG_CACHE_HOME/clipsim/history" ]; then
    echo "FAIL: --save failed, history file is missing or empty."
    exit 1
fi

# 7. Test --remove (-r)
echo "Testing --remove..."
# Remove the newest item
$CLIPSIM_BIN -r 0
sleep 0.5
NEW_PRINT_OUT=$($CLIPSIM_BIN -p)
# The output lines should have decreased
OLD_LINES=$(echo "$PRINT_OUT" | wc -l)
NEW_LINES=$(echo "$NEW_PRINT_OUT" | wc -l)
if [ "$NEW_LINES" -ge "$OLD_LINES" ]; then
    echo "FAIL: --remove did not decrease the number of history entries."
    exit 1
fi

# 8. Test --help (-h)
echo "Testing --help..."
# Help exits with status 1 if not stdout, or 0 if stdout. main.c directs -h to stdout.
HELP_OUT=$($CLIPSIM_BIN -h)
if ! echo "$HELP_OUT" | grep -q "Available commands:"; then
    echo "FAIL: --help did not output the expected usage text."
    exit 1
fi

echo "--- All tests passed successfully! ---"
# setsid -f clipsim &
