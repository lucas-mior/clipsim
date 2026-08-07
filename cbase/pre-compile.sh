#!/bin/sh -e

# shellcheck disable=SC2086

set -e

compiler=${1:-cc}
shift || true

dir=$(dirname "$(readlink -f "$0")")

compiler_object_suffix () {
    compiler_suffix=$(printf "%s" "$1"         | sed -E 's|.*/||; s|[^[:alnum:]_]+|_|g; s|^_+||; s|_+$||')

    if [ -z "$compiler_suffix" ]; then
        compiler_suffix=cc
    fi

    printf "%s" "$compiler_suffix"
}

option_remove () {
    echo "$1" | sed -E "s| *$2 +| |g"
}

with_toy_cc () {
    toy_compiler="$1"
    compiler_macro=$(echo "$toy_compiler" | tr '[:lower:]' '[:upper:]')
    compiler_macro="__${compiler_macro}__"
    shift
    args="$*"

    while ! problem=$($toy_compiler                       "-D${compiler_macro}" -D__attribute=__attribute__                       $args 2>&1); do
        problem=$(echo "$problem" | head -n 1 | tr -d "'")

        sleep 0.4
        if echo "$problem" | grep -Eq "unknown (argument|option)"; then
            arg=$(echo "$problem" | awk '{print $NF}')
            printf "
Removing argument %s...
" "$arg" >&2
            args=$(option_remove "$args" "$arg")
        elif echo "$problem" | grep -q "unknown file extension:"; then
            arg=$(echo "$problem" | awk '{print $NF}')
            printf "
Removing argument %s...
" "$arg" >&2
            args=$(option_remove "$args" "$arg")
        else
            printf "

Error compiling with %s:

%s

"                    "$toy_compiler" "$problem" >&2
            return 1
        fi
    done

    return 0
}

object="$dir/cbase-$(compiler_object_suffix "$compiler").o"
cmdfile="$object.cmd"
header="$dir/cbase.h"
cmdline="$compiler $* -DCBASE_IMPLEMENT=1 -x c -c $header -o $object"

object_stale () {
    if [ ! -f "$object" ]; then
        return 0
    fi

    if [ "$dir/pre-compile.sh" -nt "$object" ]; then
        return 0
    fi

    if ! printf "%s
" "$cmdline" | cmp -s "$cmdfile" -; then
        return 0
    fi

    find "$dir" -iname "*.[ch]" -newer "$object" -print -quit | grep -q .
}

compile_object () {
    case "$compiler" in
    chibicc|cproc)
        with_toy_cc "$compiler" "$@" \
            -DCBASE_IMPLEMENT=1 -x c -c "$header" -o "$object"
        ;;
    *)
        $compiler "$@" -DCBASE_IMPLEMENT=1 -x c -c "$header" -o "$object"
        ;;
    esac
}

if object_stale; then
    compile_object "$@"
    printf "%s
" "$cmdline" > "$cmdfile"
fi

printf "%s
" "$object"
