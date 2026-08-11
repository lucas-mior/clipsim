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
    cross)
        CC="zig cc"
        ;;
    *)
        CC="${CC:-cc}"
        ;;
    esac

    executable=$(echo "$CC" | awk '{print $1}')
    if ! command -v "$executable" > /dev/null 2>&1; then
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

gcc_flags_to_msvc() {
    compiler=clang-cl
    result=""
    next_is_linker_flag=0

    case "${1:-}" in
    clang-cl|cl)
        compiler=$1
        shift
        ;;
    esac

    for flag do
        if [ "$next_is_linker_flag" -eq 1 ]; then
            next_is_linker_flag=0
        else
            case "$flag" in
            -I*)
                path=${flag#-I}
                if command_exists cygpath; then
                    path=$(cygpath -m "$path" 2>/dev/null || printf '%s\n' "$path")
                fi
                case "$compiler" in
                clang-cl)
                    flag="/clang:-I$path"
                    ;;
                cl)
                    flag="/I$path"
                    ;;
                esac
                ;;
            -D*)
                flag="/D${flag#-D}"
                ;;
            -U*)
                flag="/U${flag#-U}"
                ;;
            -std=*)
                flag="/std:${flag#-std=}"
                ;;
            -g|-g[0-9]*)
                flag="/Z7"
                ;;
            -O0)
                case "$compiler" in
                clang-cl) flag="/clang:-O0" ;;
                cl) flag="/Od" ;;
                esac
                ;;
            -Og)
                case "$compiler" in
                clang-cl) flag="/clang:-Og" ;;
                cl) flag="/Od" ;;
                esac
                ;;
            -O1)
                case "$compiler" in
                clang-cl) flag="/clang:-O1" ;;
                cl) flag="/O1" ;;
                esac
                ;;
            -O2|-O3|-Ofast)
                case "$compiler" in
                clang-cl) flag="/clang:-O2" ;;
                cl) flag="/O2" ;;
                esac
                ;;
            -Os|-Oz)
                case "$compiler" in
                clang-cl) flag="/clang:$flag" ;;
                cl) flag="/O1" ;;
                esac
                ;;
            -Wall)
                case "$compiler" in
                clang-cl) flag="/W4 /clang:-Wno-constant-logical-operand" ;;
                cl) flag="/W4" ;;
                esac
                ;;
            -Wextra|-Wpedantic)
                continue
                ;;
            -Wfatal-errors|-Wno-*|-W*)
                case "$compiler" in
                clang-cl) flag="/clang:$flag" ;;
                cl) continue ;;
                esac
                ;;
            -fsanitize=undefined)
                continue
                ;;
            -flto|-march=*|-ftree-vectorize)
                case "$compiler" in
                clang-cl) flag="/clang:$flag" ;;
                cl) continue ;;
                esac
                ;;
            -lm|-lpthread)
                case "$compiler" in
                clang-cl)
                    case "$CLANG_CL_TARGET" in
                    *linux*|*darwin*|*bsd*)
                        flag="-Xlinker $flag"
                        ;;
                    *)
                        continue
                        ;;
                    esac
                    ;;
                cl)
                    continue
                    ;;
                esac
                ;;
            -Xlinker)
                next_is_linker_flag=1
                ;;
            esac
        fi

        if [ -z "$result" ]; then
            result=$flag
        else
            result="$result $flag"
        fi
    done

    printf '%s\n' "$result"
}


test_run_binary () {
    test_exe=$1

    if [ -n "${TEST_STDIN:-}" ]; then
        "$test_exe" < "$TEST_STDIN"
    else
        "$test_exe" < /dev/null
    fi
}

test_debugger () {
    test_exe=$1

    if command_exists gdb; then
        gdb --quiet \
            -ex run -ex backtrace -ex quit \
            "$test_exe" < /dev/null 2>&1 || true
    elif command_exists lldb; then
        lldb \
            --batch \
            --one-line "run" \
            --one-line "bt" \
            -- "$test_exe" < /dev/null 2>&1 || true
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
        case "${CC:-}" in
        clang-cl|*/clang-cl)
            test_exe_suffix=_test.exe
            ;;
        cl|*/cl|cl.exe|*/cl.exe)
            test_exe_suffix=_test.exe
            ;;
        *)
            test_exe_suffix=_test
            ;;
        esac
    fi

    if [ "${TEST_TMPDIR+set}" = set ]; then
        test_tmpdir=$TEST_TMPDIR
    else
        case "${CC:-}" in
        clang-cl|*/clang-cl|cl|*/cl|cl.exe|*/cl.exe)
            case "$(uname -a)" in
            *MINGW*|*MSYS*|*CYGWIN*)
                test_tmpdir=.test-tmp
                ;;
            *)
                test_tmpdir=${TMPDIR:-/tmp}
                ;;
            esac
            ;;
        *)
            test_tmpdir=${TMPDIR:-/tmp}
            ;;
        esac
    fi

    printf '%s/%s%s\n' \
        "$test_tmpdir" \
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
    test_cmd_flags="$CPPFLAGS $TEST_CPPFLAGS $CFLAGS $TEST_CFLAGS"
    test_added_flags=""
    test_ldflags="$TEST_LDFLAGS"
    test_tail_ldflags="$LDFLAGS"
    test_run_after_compile=1
    test_msvc_compiler=

    mkdir -p "$(dirname "$test_exe")"

    printf "\nTesting ${RED}%s${RES} ...\n" "$test_src"

    if [ -n "${TEST_WINDOWS_SOURCE_PATTERN:-}" ] \
            && echo "$test_src" | grep -Eq "$TEST_WINDOWS_SOURCE_PATTERN"; then
        if ! command_exists zig; then
            return 0
        fi

        test_cc="zig cc"
        test_cmdline="$test_cc $test_cmd_flags"
        test_cmdline=$(option_remove "$test_cmdline" "-D_GNU_SOURCE")
        test_cmdline="$test_cmdline -target x86_64-windows-gnu"
        test_run_after_compile=${TEST_WINDOWS_RUN:-1}
    else
        case "$test_cc" in
        clang-cl|*/clang-cl)
            test_msvc_compiler=clang-cl
            if [ -z "$CLANG_CL_TARGET" ]; then
                case "$(uname -a)" in
                *Linux*|*Darwin*|*BSD*)
                    CLANG_CL_TARGET=$(cc -dumpmachine 2>/dev/null || true)
                    ;;
                esac
            fi
            if [ -n "$CLANG_CL_TARGET" ]; then
                test_cmd_flags="$test_cmd_flags --target=$CLANG_CL_TARGET"
            fi
            test_cmd_flags=$(gcc_flags_to_msvc "$test_msvc_compiler" $test_cmd_flags)
            ;;
        cl|*/cl|cl.exe|*/cl.exe)
            test_msvc_compiler=cl
            test_cmd_flags=$(gcc_flags_to_msvc "$test_msvc_compiler" $test_cmd_flags)
            ;;
        esac
        test_cmdline="$test_cc $test_cmd_flags"
    fi

    if [ "${TEST_DISABLE_UNUSED_VARIABLE_WARNING:-1}" != 0 ]; then
        test_added_flags="$test_added_flags -Wno-unused-variable"
    fi

    if [ "${TEST_DEFINE_MODULE:-1}" != 0 ]; then
        test_added_flags="$test_added_flags -DTESTING_$test_module=1"
    fi

    if [ "${TEST_DEFINE_TESTING:-1}" != 0 ]; then
        test_added_flags="$test_added_flags -DTESTING=1"
    fi

    test_added_flags="$test_added_flags $TEST_EXTRA_DEFS"
    if [ -n "$test_msvc_compiler" ]; then
        test_added_flags=$(gcc_flags_to_msvc "$test_msvc_compiler" $test_added_flags)
        test_flags=$(gcc_flags_to_msvc "$test_msvc_compiler" $test_flags)
        test_ldflags=$(gcc_flags_to_msvc "$test_msvc_compiler" $test_ldflags)
        test_tail_ldflags=$(gcc_flags_to_msvc "$test_msvc_compiler" $test_tail_ldflags)
    fi
    test_cmdline="$test_cmdline $test_added_flags"
    if [ "$test_msvc_compiler" = cl ]; then
        test_cmdline="$test_cmdline /Fe$test_exe $test_src"
    else
        test_cmdline="$test_cmdline -o $test_exe $test_src"
    fi
    test_cmdline="$test_cmdline $test_ldflags $test_flags $test_tail_ldflags"

    trace_on
    if $test_cmdline < /dev/null; then
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
