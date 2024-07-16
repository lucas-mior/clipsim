#!/bin/sh

# clipboard for dwmblocks2

display () {
    printf "📋 "
    sleep 0.1
    result="$(timeout 0.5s xclip -selection clipboard -o -t TARGETS)"

    if echo "$result" | grep -q "image/png"; then
        printf "PNG\n"
    elif [ -n "$result" ]; then
        content="$(timeout 1s dash -c \
                   "xclip -selection clipboard -o | head -n 100")"
        echo "$content" \
            | tr '\n\0' ' ' \
            | sed -E 's/^\s+//g;
                      s/([^^])\s+/\1 /g;
                      s/\s+$//g;
                      s/(.{14}\S).{4,}(\S.{5})/\1 ... \2/g;'
    fi
}

case $1 in
    1) pgrep clipsim  > /dev/null || setsid -f clipsim -d                  ; display ;;
    2) clipsim --save > /dev/null && dunstify "📋 clipsim" "History saved" ; display ;;
    6) setsid -f "$TERMINAL" -e "$EDITOR" "$0"                                 ; display ;;
    *) display ;;
esac
