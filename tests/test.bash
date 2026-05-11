#!/usr/bin/env bash

alias trace_on='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

if [ -n "$BASH_VERSION" ]; then
    # shellcheck disable=SC3044
    shopt -s expand_aliases
fi

# shellcheck disable=2317
# shellcheck disable=2086

old_xdg_cache_home="$XDG_CACHE_HOME"
interval=0.3

clipsim_was_running=false
if killall -SIGTERM clipsim 2>/dev/null; then
    clipsim_was_running=true
    sleep $interval
    killall -SIGKILL clipsim
fi
if killall -SIGTERM clipsim_debug 2>/dev/null; then
    clipsim_was_running=true
    sleep $interval
    killall -SIGKILL clipsim_debug
fi

set -e

dir=$(dirname "$(realpath "$0")")
cd "$dir" || exit

clipsim_bin="../bin/clipsim_debug"
TEST_DIR="/tmp/clipsim_test_bash"
XDG_CACHE_HOME="$TEST_DIR/.cache"
../build.sh debug || exit 1

rm -rf "$TEST_DIR"
mkdir -p "$XDG_CACHE_HOME"
export XDG_CACHE_HOME

$clipsim_bin --daemon &
clipsim_daemon_pid=$!

cleanup () {
    kill -SIGKILL $clipsim_daemon_pid 2>/dev/null
    rm -rf "$TEST_DIR"

    if [ "$clipsim_was_running" = true ]; then
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

echo "Compiling explicit INCR tests mock owner..."
incr_owner_c="./incr_owner.c"
incr_owner_bin="./incr_owner"

trace_on
gcc -I../cbase -I../ -O2 $incr_owner_c -lX11 -lm -o $incr_owner_bin
trace_off

echo "Triggering normal INCR..."
$incr_owner_bin 0 &
pid_incr_small=$!
sleep 1

$clipsim_bin -p > "$TEST_DIR/incr_normal_dump"
if ! grep -q "small_incr_test" "$TEST_DIR/incr_normal_dump"; then
    echo "FAIL: Normal INCR transfer was not saved to history."
    exit 1
fi
wait $pid_incr_small 2>/dev/null || true

echo "Triggering too large INCR..."
$incr_owner_bin 1 &
pid_incr_large=$!
sleep 1

$clipsim_bin -p > "$TEST_DIR/incr_large_dump"
if grep -q "AAAAAA" "$TEST_DIR/incr_large_dump"; then
    echo "FAIL: Large INCR transfer exceeded ENTRY_MAX_LENGTH but was incorrectly saved."
    exit 1
fi
wait $pid_incr_large 2>/dev/null || true
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

echo "not me" | xclip -selection clipboard
sleep $interval

$clipsim_bin -c -1
sleep $interval

$clipsim_bin -p > "$TEST_DIR/before_xclip"
sleep $interval

CLIP_DATA=$(xclip -o -selection clipboard)
sleep $interval

if [ "$CLIP_DATA" != "not me" ]; then
    echo "FAIL: --copy resulted in an empty clipboard."
    exit 1
fi

$clipsim_bin -s
sleep $interval
if [ ! -s "$XDG_CACHE_HOME/clipsim/history" ]; then
    echo "FAIL: --save failed, history file is missing or empty."
    exit 1
fi

$clipsim_bin -p > "$TEST_DIR/before_remove"
sleep $interval
$clipsim_bin -r 0
sleep $interval
$clipsim_bin -p > "$TEST_DIR/after_remove"

OLD_LINES=$(tr -cd '\0' < "$TEST_DIR/before_remove" | wc -c)
NEW_LINES=$(tr -cd '\0' < "$TEST_DIR/after_remove"  | wc -c)
if [ "$NEW_LINES" -ge "$OLD_LINES" ]; then
    echo "FAIL: --remove did not decrease the number of history entries."
    exit 1
fi

echo "Triggering unresponsive application holding the clipboard..."
unresponsive_c="./unresponsive_owner.c"
unresponsive_bin="./unresponsive_owner"

trace_on

gcc -O2 $unresponsive_c -lX11 -lm -o $unresponsive_bin
$unresponsive_bin &
unresponsive_pid=$!

trace_off
sleep 1

if ! timeout 2 $clipsim_bin -i 0 > /dev/null 2>&1; then
    echo "FAIL: Daemon deadlocked trying to read from unresponsive clipboard owner."
    kill -SIGKILL $unresponsive_pid 2>/dev/null
    exit 1
fi

kill -SIGKILL $unresponsive_pid 2>/dev/null
sleep $interval

echo "All tests passed successfully!"

reset
