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


test_run_binary () {
    test_exe=$1

    if [ -n "${TEST_STDIN:-}" ]; then
        "$test_exe" < "$TEST_STDIN"
    else
        "$test_exe"
    fi
}

test_debugger () {
    test_exe=$1

    if command_exists gdb; then
        gdb --quiet \
            -ex run -ex backtrace -ex quit \
            "$test_exe" 2>&1 || true
    elif command_exists lldb; then
        lldb \
            --batch \
            --one-line "run" \
            --one-line "bt" \
            -- "$test_exe" 2>&1 || true
    fi

    return 0
}

test_source_matches_filter () {
    test_src=$1
    test_filter=$2

    if [ -z "$test_filter" ]; then
        return 0
    fi

    test_name=$(basename "$test_src")
    test_module=${test_name%.c}
    test_filter_base=$(basename "$test_filter")
    test_filter_module=${test_filter_base%.c}

    if [ "$test_src" = "$test_filter" ] \
            || [ "$test_name" = "$test_filter_base" ] \
            || [ "$test_module" = "$test_filter_module" ]; then
        return 0
    fi

    return 1
}

test_source_is_excluded () {
    test_src=$1
    test_name=$(basename "$test_src")
    test_module=${test_name%.c}
    test_exclude_pattern="(^|/)stc/"

    if [ "${TEST_SKIP_MAIN:-1}" != 0 ] \
            && echo "$test_name" | grep -Eq '^main[^/]*\.c$'; then
        return 0
    fi

    if [ -n "${TEST_EXCLUDE_PATTERN:-}" ]; then
        test_exclude_pattern="$test_exclude_pattern|$TEST_EXCLUDE_PATTERN"
    fi

    if echo "$test_src" | grep -Eq "$test_exclude_pattern"; then
        return 0
    fi

    if [ -z "${TEST_FILTER:-}" ] \
            && [ -n "${TEST_EXCLUDE_UNFILTERED_PATTERN:-}" ] \
            && echo "$test_src" \
                | grep -Eq "$TEST_EXCLUDE_UNFILTERED_PATTERN"; then
        return 0
    fi

    if [ "${TEST_REQUIRE_TESTING_MARKER:-1}" != 0 ] \
            && ! grep -q "TESTING_$test_module" "$test_src"; then
        return 0
    fi

    return 1
}

test_executable_path () {
    test_module=$1

    if [ -n "${TEST_EXE_PATH:-}" ]; then
        printf '%s\n' "$TEST_EXE_PATH"
        return 0
    fi

    if [ "${TEST_EXE_SUFFIX+set}" = set ]; then
        test_exe_suffix=$TEST_EXE_SUFFIX
    else
        test_exe_suffix=_test
    fi

    printf '%s/%s%s\n' \
        "${TEST_TMPDIR:-${TMPDIR:-/tmp}}" \
        "$test_module" \
        "$test_exe_suffix"
}

test_compile_and_run_source () {
    test_src=$1
    test_name=$(basename "$test_src")
    test_module=${test_name%.c}
    test_exe=$(test_executable_path "$test_module")
    test_flags=$(awk '/flags:/ { $1=$2=""; print $0 }' "$test_src")
    test_cc=$CC
    test_run_after_compile=1

    mkdir -p "$(dirname "$test_exe")"

    printf "\nTesting ${RED}%s${RES} ...\n" "$test_src"

    if [ -n "${TEST_WINDOWS_SOURCE_PATTERN:-}" ] \
            && echo "$test_src" | grep -Eq "$TEST_WINDOWS_SOURCE_PATTERN"; then
        if ! command_exists zig; then
            return 0
        fi

        test_cc="zig cc"
        test_cmdline="$test_cc $CPPFLAGS $TEST_CPPFLAGS $CFLAGS $TEST_CFLAGS"
        test_cmdline=$(option_remove "$test_cmdline" "-D_GNU_SOURCE")
        test_cmdline="$test_cmdline -target x86_64-windows-gnu"
        test_run_after_compile=${TEST_WINDOWS_RUN:-1}
    else
        test_cmdline="$test_cc $CPPFLAGS $TEST_CPPFLAGS $CFLAGS $TEST_CFLAGS"
    fi

    if [ "${TEST_DISABLE_UNUSED_VARIABLE_WARNING:-1}" != 0 ]; then
        test_cmdline="$test_cmdline -Wno-unused-variable"
    fi

    if [ "${TEST_DEFINE_MODULE:-1}" != 0 ]; then
        test_cmdline="$test_cmdline -DTESTING_$test_module=1"
    fi

    if [ "${TEST_DEFINE_TESTING:-1}" != 0 ]; then
        test_cmdline="$test_cmdline -DTESTING=1"
    fi

    test_cmdline="$test_cmdline $TEST_EXTRA_DEFS"
    test_cmdline="$test_cmdline -o $test_exe $test_src"
    test_cmdline="$test_cmdline $TEST_LDFLAGS $test_flags $LDFLAGS"

    trace_on
    if $test_cmdline; then
        if [ "$test_run_after_compile" != 0 ] \
                && ! test_run_binary "$test_exe"; then
            test_debugger "$test_exe"
            exit 1
        fi
    else
        exit 1
    fi
    trace_off

    return 0
}

test () {
    TEST_FILTER=${1:-}
    if [ "$#" -gt 0 ]; then
        shift
    fi

    if [ "$#" -eq 0 ]; then
        if [ "${TEST_SOURCES+set}" = set ]; then
            # shellcheck disable=SC2086
            set -- $TEST_SOURCES
        else
            set -- .
        fi
    fi

    test_roots=
    for test_root do
        if [ -e "$test_root" ]; then
            test_roots="$test_roots $test_root"
        fi
    done

    if [ -z "$test_roots" ]; then
        return 0
    fi

    if [ -z "${TEST_WINDOWS_SOURCE_PATTERN:-}" ]; then
        TEST_WINDOWS_SOURCE_PATTERN='(^|/)windows_functions\.c$'
    fi

    {
        if [ -n "${TEST_MAXDEPTH:-}" ]; then
            # shellcheck disable=SC2086
            find $test_roots -maxdepth "$TEST_MAXDEPTH" -iname "*.c"
        else
            # shellcheck disable=SC2086
            find $test_roots -iname "*.c"
        fi
    } | sort | while read -r test_src; do
        trace_off

        if ! test_source_matches_filter "$test_src" "$TEST_FILTER"; then
            continue
        fi

        if test_source_is_excluded "$test_src"; then
            continue
        fi

        test_compile_and_run_source "$test_src"
    done

    return 0
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

compile_cbase () {
    CC="${CC:-cc}"

    trace_on
    $CC -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700 -c "cbase.c" -o "cbase-${CC}.o"
    trace_off
}

if [ "$(basename "$0")" = "common.sh" ]; then
    compile_cbase
fi
