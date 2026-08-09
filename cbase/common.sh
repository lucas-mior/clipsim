# shellcheck shell=sh

# shellcheck disable=SC2086

set -e

error () {
    >&2 printf "$@"
    return
}

command_exists () {
    command -v "$1" > /dev/null 2>&1
}

if [ -n "$BASH_VERSION" ]; then
    # shellcheck disable=SC3044
    shopt -s expand_aliases
fi

alias trace_on='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

get_compiler() {
    case "$1" in
    debug|test)
        CC="${CC:-tcc}"
        ;;
    fast_feedback)
        CC="${CC:-clang}"
        ;;
    *)
        CC="${CC:-cc}"
        ;;
    esac

    if ! command -v "$CC" > /dev/null 2>&1; then
        CC=cc
    fi

    echo "$CC"
}

get_program() {
    if [ -z "$1" ]; then
        error "get_program <full_path_to_build.sh>"
        exit 1
    fi
    basename "$(readlink -f "$(dirname "$1")")"
}

needs_rebuild_includes () {
    source_file=$1

    awk '
        /^[[:space:]]*#[[:space:]]*include[[:space:]]*"/ {
            line = $0
            sub(/^[^"]*"/, "", line)
            sub(/".*$/, "", line)

            if (line ~ /\.[ch]$/) {
                print line
            }
        }
    ' "$source_file"
}

needs_rebuild_resolve_include () {
    source_file=$1
    include_file=$2
    source_dir=$(dirname "$source_file")

    if [ -f "$source_dir/$include_file" ]; then
        printf '%s\n' "$source_dir/$include_file"
        return 0
    fi

    if [ -f "$include_file" ]; then
        printf '%s\n' "$include_file"
        return 0
    fi

    return 1
}

needs_rebuild_source_is_newer () {
    target_file=$1
    source_file=$2
    seen_file=$3

    if [ ! -e "$source_file" ]; then
        return 1
    fi

    if grep -F -x -- "$source_file" "$seen_file" > /dev/null 2>&1; then
        return 1
    fi

    printf '%s\n' "$source_file" >> "$seen_file"

    if [ "$source_file" -nt "$target_file" ]; then
        return 0
    fi

    if [ ! -f "$source_file" ]; then
        return 1
    fi

    for include_file in $(needs_rebuild_includes "$source_file"); do
        resolved_file=$(
            needs_rebuild_resolve_include "$source_file" "$include_file" \
                || true
        )

        if [ -n "$resolved_file" ] \
                && needs_rebuild_source_is_newer \
                    "$target_file" "$resolved_file" "$seen_file"; then
            return 0
        fi
    done

    return 1
}

needs_rebuild () {
    target_file=$1
    shift

    if [ ! -e "$target_file" ]; then
        return 0
    fi

    seen_file=${TMPDIR:-/tmp}/needs_rebuild.$$.seen
    : > "$seen_file"

    for source_file do
        if needs_rebuild_source_is_newer \
                "$target_file" "$source_file" "$seen_file"; then
            rm -f "$seen_file"
            return 0
        fi
    done

    rm -f "$seen_file"

    if [ -d cbase ] \
            && find cbase -type f -newer "$target_file" | grep -q .; then
        return 0
    fi

    return 1
}

target_supported () {
    target_list=$1
    wanted=$2

    printf '%s\n' "$target_list" | awk -v wanted="$wanted" '
        {
            line = $0
            sub(/^# /, "", line)
        }
        line == wanted { found = 1 }
        END { exit !found }
    '
}

option_remove() {
    remove=$2
    result=""

    for option in $1; do
        if [ "$option" = "$remove" ]; then
            continue
        fi

        if [ -z "$result" ]; then
            result=$option
        else
            result="$result $option"
        fi
    done

    printf '%s\n' "$result"
}

build_tags () {
    if [ "$#" -eq 0 ]; then
        set -- .
    fi

    if command_exists ctags; then
        find "$@" -iname "*.[ch]" -print0 \
            | xargs --verbose -0 ctags --kinds-C=+l+d || true
    fi

    if [ -f tags ] && command_exists vtags.sed; then
        vtags.sed tags | sort | uniq > .tags.vim || true
    fi
}

install_file() {
    mode=$1
    src=$2
    dst=$3
    dst_dir=$(dirname "$dst")

    mkdir -p "$dst_dir"
    install -m "$mode" "$src" "$dst"
}

install_opt () {
    mode="$1"
    file="$2"
    dest="$3"

    if [ -f "$file" ]; then
        install "$mode" "$file" "$dest"
    elif [ -d "$file" ]; then
        install "$mode" "$dest"
        cp -rp "$file/." "$dest/"
    fi
}

uninstall_opt () {
    file="$1"
    dest="$2"

    if [ -e "$file" ]; then
        rm -rf "$dest"
    fi
}

compile_with_other () {
    compiler=$1
    compiler_macro=$(echo "$compiler" | tr '[:lower:]' '[:upper:]' | tr ' ' '_')
    compiler_macro="__${compiler_macro}__"
    shift
    args="$*"

    trace_on
    while ! problem=$( \
            $compiler "-D${compiler_macro}" \
            ${COMPILE_WITH_OTHER_EXTRA_DEFS:-} \
            $args 2>&1 \
        ); do
        trace_off
        problem=$(echo "$problem" | head -n 1 | tr -d "'")

        sleep 0.4
        if echo "$problem" | grep -Eq "unknown (argument|option)"; then
            arg=$(echo "$problem" | awk '{print $NF}')
            printf "\nRemoving argument %s...\n" "$arg"
            args=$(option_remove "$args" "$arg")
        elif echo "$problem" | grep -q "unknown file extension:"; then
            arg=$(echo "$problem" | awk '{print $NF}')
            printf "\nRemoving argument %s...\n" "$arg"
            args=$(option_remove "$args" "$arg")
        else
            printf "\n\nError compiling with %s:\n\n%s\n\n" \
                "$compiler" "$problem"
            return 1
        fi
        printf "\n"
        trace_on
    done

    return 0
}

compile_cbase () {
    CC="${CC:-cc}"

    trace_on
    $CC -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -c "cbase.c" -o "cbase-${CC}.o"
    trace_off
}

if [ "$(basename "$0")" = "common.sh" ]; then
    compile_cbase
fi
