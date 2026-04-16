#!/usr/bin/env bash

old_xdg_cache_home="$XDG_CACHE_HOME"
interval=0.3

CLIPSIM_WAS_RUNNING=false
if killall -SIGTERM clipsim 2>/dev/null; then
    CLIPSIM_WAS_RUNNING=true
    sleep $interval
    killall -SIGKILL clipsim
fi

set -e

clipsim_bin="bin/clipsim_debug"
TEST_DIR="/tmp/clipsim_test_bash"
XDG_CACHE_HOME="$TEST_DIR/.cache"
./build.sh debug || exit 1

rm -rf "$TEST_DIR"
mkdir -p "$XDG_CACHE_HOME"
export XDG_CACHE_HOME

$clipsim_bin --daemon &
DAEMON_PID=$!

cleanup () {
    kill -SIGKILL $DAEMON_PID 2>/dev/null
    rm -rf "$TEST_DIR"

    if [ "$CLIPSIM_WAS_RUNNING" = true ]; then
        XDG_CACHE_HOME="$old_xdg_cache_home" \
            setsid -f clipsim -d > /dev/null 2>&1
    fi
}
trap cleanup EXIT

sleep 1

echo "Triggering clipboard changes..."

echo "first_test_string" | xclip -selection clipboard
sleep $interval
echo "second_test_string" | xclip -selection clipboard
sleep $interval

echo "Triggering clipboard image..."
IMAGE_FILE="$TEST_DIR/test_image.png"
printf '
iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKAQAAAAClSfIQAAAAIGNIUk0AAHomAACAhAAA+gAAAIDo
AAB1MAAA6mAAADqYAAAXcJy6UTwAAAACYktHRAAB3YoTpAAAAAd0SU1FB+oEEBEmH42gJI0AAAAl
dEVYdGRhdGU6Y3JlYXRlADIwMjYtMDQtMTZUMTc6Mzg6MzErMDA6MDABg167AAAAJXRFWHRkYXRl
Om1vZGlmeQAyMDI2LTA0LTE2VDE3OjM4OjMxKzAwOjAwcN7mBwAAACh0RVh0ZGF0ZTp0aW1lc3Rh
bXAAMjAyNi0wNC0xNlQxNzozODozMSswMDowMCfLx9gAAAALSURBVAjXY2DABwAAHgABboVHMgAA
AABJRU5ErkJggg==' | tr -d '\n' | base64 -d > "$IMAGE_FILE"
xclip -selection clipboard -t image/png -i "$IMAGE_FILE"
sleep $interval

echo "Triggering unsupported clipboard format..."
head -n 5 /dev/random > $TEST_DIR/some_binary_format
reset
xclip -selection clipboard -t application/x-custom-format $TEST_DIR/some_binary_format
od $TEST_DIR/some_binary_format > $TEST_DIR/some_binary_format.txt
sleep $interval

echo "Triggering large clipboard data (INCR)..."
large_file="$TEST_DIR/large_file.txt"
dd if=/dev/zero of="$large_file" bs=1M count=2 2>/dev/null
xclip -selection clipboard -i "$large_file"
sleep $interval

echo "recovery_target" | xclip -quiet -selection clipboard &
pid_xclip=$!
sleep $interval
kill -SIGTERM $pid_xclip
sleep $interval

recovered=$(xclip -o -selection clipboard)
if [ "$recovered" != "recovery_target" ]; then
    echo "FAIL: Empty clipboard did not trigger recovery of the last valid entry."
    exit 1
fi

sleep $interval
clipsim_history="$TEST_DIR/dump"
$clipsim_bin -p > "$clipsim_history"
sleep $interval

if ! grep -q "first_test_string" "$clipsim_history" ; then
    echo "FAIL: --print did not output expected text history."
    exit 1
fi

if ! grep -q "\.png" "$clipsim_history"; then
    echo "FAIL: --print did not output expected image history."
    exit 1
fi

od $clipsim_history > $clipsim_history.txt
if grep -Fq -f "$TEST_DIR/some_binary_format.txt" "$clipsim_history.txt"; then
    echo "FAIL: Unsupported format was incorrectly added to history."
    exit 1
fi

INFO_OUT=$($clipsim_bin -i 0)
sleep $interval
if ! echo "$INFO_OUT" | grep -q "Length:"; then
    echo "FAIL: --info did not output the expected length metadata."
    exit 1
fi

$clipsim_bin -c 0
sleep $interval
CLIP_DATA=$(xclip -o -selection clipboard)
if [ -z "$CLIP_DATA" ]; then
    echo "FAIL: --copy resulted in an empty clipboard."
    exit 1
fi

$clipsim_bin -s
sleep $interval
if [ ! -s "$XDG_CACHE_HOME/clipsim/history" ]; then
    echo "FAIL: --save failed, history file is missing or empty."
    exit 1
fi

$clipsim_bin -r 0
sleep $interval
$clipsim_bin -p > "$TEST_DIR/new_dump"

OLD_LINES=$(wc -l "$clipsim_history")
NEW_LINES=$(wc -l "$TEST_DIR/new_dump")
if [ "$NEW_LINES" -ge "$OLD_LINES" ]; then
    echo "FAIL: --remove did not decrease the number of history entries."
    exit 1
fi

echo "All tests passed successfully!"
