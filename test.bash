#!/usr/bin/env bash

# kill any running clipsim so that they dont get on the way
old_xdg_cache_home="$XDG_CACHE_HOME"

CLIPSIM_WAS_RUNNING=false
if killall -SIGKILL clipsim 2>/dev/null; then
    CLIPSIM_WAS_RUNNING=true
fi

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

    # restart clipsim only if it was killed previously
    if [ "$CLIPSIM_WAS_RUNNING" = true ]; then
        XDG_CACHE_HOME="$old_xdg_cache_home" \
            setsid -f clipsim -d > /dev/null 2>&1
    fi
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

# 1. CLIPBOARD_TEXT: Standard text entries
echo -n "first_test_string" | xclip -selection clipboard
sleep 0.5
echo -n "second_test_string" | xclip -selection clipboard
sleep 0.5

# 2. CLIPBOARD_IMAGE: Image entries
echo "Triggering clipboard image..."
IMAGE_FILE="$TEST_DIR/test_image.png"
# Create a valid 1x1 transparent PNG using base64
echo "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==" | base64 -d > "$IMAGE_FILE"
xclip -selection clipboard -t image/png -i "$IMAGE_FILE"
sleep 0.5

# 3. CLIPBOARD_OTHER: Unsupported format entries
echo "Triggering unsupported clipboard format..."
echo -n "unsupported_data" | xclip -selection clipboard -t application/x-custom-format
sleep 0.5

# 4. CLIPBOARD_LARGE: Large data to trigger INCR
echo "Triggering large clipboard data (INCR)..."
LARGE_FILE="$TEST_DIR/large_file.txt"
dd if=/dev/zero of="$LARGE_FILE" bs=1M count=2 2>/dev/null
xclip -selection clipboard -i "$LARGE_FILE"
sleep 0.5

# Prepare target for the recovery test
echo -n "recovery_target" | xclip -selection clipboard
sleep 0.5

# 5. CLIPBOARD_ERROR: Empty clipboard (triggers history_recover(-1))
echo "Triggering empty clipboard..."
xclip -selection clipboard -i /dev/null
sleep 0.5

# Verify the CLIPBOARD_ERROR successfully recovered the last valid entry
RECOVERED_DATA=$(xclip -o -selection clipboard)
if [ "$RECOVERED_DATA" != "recovery_target" ]; then
    echo "FAIL: Empty clipboard did not trigger recovery of the last valid entry."
    exit 1
fi

# Assertions against the saved history
PRINT_OUT=$($clipsim_bin -p)

# Verify text was added
if ! echo "$PRINT_OUT" | grep -q "first_test_string"; then
    echo "FAIL: --print did not output expected text history."
    exit 1
fi

# Verify image was added (prints the .png path)
if ! echo "$PRINT_OUT" | grep -q "\.png"; then
    echo "FAIL: --print did not output expected image history."
    exit 1
fi

# Verify unsupported data was ignored
if echo "$PRINT_OUT" | grep -q "unsupported_data"; then
    echo "FAIL: Unsupported format was incorrectly added to history."
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

echo "All tests passed successfully!"
