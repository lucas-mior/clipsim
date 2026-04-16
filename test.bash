#!/usr/bin/env bash

old_xdg_cache_home="$XDG_CACHE_HOME"
killall -SIGKILL clipsim

set -e

clipsim_bin="bin/clipsim"
TEST_DIR="/tmp/clipsim_full_test"
XDG_CACHE_HOME="$TEST_DIR/.cache"
./build.sh || exit 1

rm -rf "$TEST_DIR"
mkdir -p "$XDG_CACHE_HOME"
export XDG_CACHE_HOME

ls /tmp/clipsim_full_test
$clipsim_bin --daemon &
DAEMON_PID=$!

cleanup () {
    kill -SIGKILL $DAEMON_PID 2>/dev/null
    rm -rf "$TEST_DIR"
    XDG_CACHE_HOME="$old_xdg_cache_home" \
        setsid -f clipsim -d > /dev/null 2>&1
}
trap cleanup EXIT

sleep 1

timeout 1s $clipsim_bin --daemon
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

PRINT_OUT=$($clipsim_bin -p)
if ! echo "$PRINT_OUT" | grep -q "first_test_string"; then
    echo "FAIL: --print did not output expected history."
    exit 1
fi

INFO_OUT=$($clipsim_bin -i 0)
if ! echo "$INFO_OUT" | grep -q "Length:"; then
    echo "FAIL: --info did not output the expected length metadata."
    exit 1
fi

$clipsim_bin -c 0
sleep 0.5
CLIP_DATA=$(xclip -o -selection clipboard)
if [ -z "$CLIP_DATA" ]; then
    echo "FAIL: --copy resulted in an empty clipboard."
    exit 1
fi

$clipsim_bin -s
sleep 0.5
if [ ! -s "$XDG_CACHE_HOME/clipsim/history" ]; then
    echo "FAIL: --save failed, history file is missing or empty."
    exit 1
fi

$clipsim_bin -r 0
sleep 0.5
NEW_PRINT_OUT=$($clipsim_bin -p)

OLD_LINES=$(echo "$PRINT_OUT" | wc -l)
NEW_LINES=$(echo "$NEW_PRINT_OUT" | wc -l)
if [ "$NEW_LINES" -ge "$OLD_LINES" ]; then
    echo "FAIL: --remove did not decrease the number of history entries."
    exit 1
fi

HELP_OUT=$($clipsim_bin -h)
if ! echo "$HELP_OUT" | grep -q "Available commands:"; then
    echo "FAIL: --help did not output the expected usage text."
    exit 1
fi
