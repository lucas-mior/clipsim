# shellcheck shell=sh

# shellcheck disable=SC2086

set -e

case ${RED:-} in
''|'\033[01;38;2;255;000;000'|'\033[01;38;2;255;000;000m')
    RED=$(printf '\033[01;38;2;255;000;000m')
    ;;
esac

case ${RES:-} in
''|'\033[0;m'|'\033[0m')
    RES=$(printf '\033[0m')
    ;;
esac

export RED RES

error () {
    >&2 printf "$@"
    return
}

# shellcheck source=./cbase/functions_forbidden.sh
. ./cbase/functions_forbidden.sh

common_command_exists () {
    command -v "$1" > /dev/null 2>&1
}

if [ -n "$BASH_VERSION" ]; then
    # shellcheck disable=SC3044
    shopt -s expand_aliases
fi

alias trace_on='set -x'
# alias trace_off='set -x'
alias trace_off='{ set +x; } 2>/dev/null'

common_get_compiler() {
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

common_get_program() {
    if [ -z "$1" ]; then
        error "common_get_program <full_path_to_build.sh>"
        exit 1
    fi
    basename "$(readlink -f "$(dirname "$1")")"
}

cross_targets=
if common_command_exists zig; then
    cross_targets=$(zig targets \
                    | sed -n '/\.libc = \.{/,/},/ s/^[[:space:]]*"\(.*\)".*/\1/p' \
                    | sort \
                    | grep -v "32" \
                    | grep -v -- "^arc-" \
                    | grep -v -- "^armeb-" \
                    | grep -v -- "^m68k-" \
                    | grep -v -- "^loongarch64-linux-gnusf" \
                    | grep -v -- "^sparc-" \
                    | grep -v -- "^sparc64-" \
                    | grep -v -- "^csky-")
fi
echo "cross_targets = $cross_targets" > /dev/null

common_outdated_includes () {
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

common_outdated_resolve_include () {
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

common_outdated_source_is_newer () {
    rebuild_target=$1
    source_file=$2
    seen_file=$3

    if [ ! -e "$source_file" ]; then
        return 1
    fi

    if grep -F -x -- "$source_file" "$seen_file" > /dev/null 2>&1; then
        return 1
    fi

    printf '%s\n' "$source_file" >> "$seen_file"

    if [ "$source_file" -nt "$rebuild_target" ]; then
        return 0
    fi

    if [ ! -f "$source_file" ]; then
        return 1
    fi

    for include_file in $(common_outdated_includes "$source_file"); do
        resolved_file=$(
            common_outdated_resolve_include "$source_file" "$include_file" \
                || true
        )

        if [ -n "$resolved_file" ] \
                && common_outdated_source_is_newer \
                    "$rebuild_target" "$resolved_file" "$seen_file"; then
            return 0
        fi
    done

    return 1
}

common_outdated () {
    rebuild_target=$1
    shift

    if [ ! -e "$rebuild_target" ]; then
        return 0
    fi

    seen_file=${TMPDIR:-/tmp}/common_outdated.$$.seen
    : > "$seen_file"

    for source_file do
        if common_outdated_source_is_newer \
                "$rebuild_target" "$source_file" "$seen_file"; then
            rm -f "$seen_file"
            return 0
        fi
    done

    rm -f "$seen_file"

    if [ -d cbase ] \
            && find cbase -type f -newer "$rebuild_target" | grep -q .; then
        return 0
    fi

    return 1
}

common_target_supported () {
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


common_build_parse_args () {
    mode=${1:-debug}
    target=${2:-}

    if [ -n "${1:-}" ] && [ -f "$1" ]; then
        mode=debug
        target=$1
    fi

    target_line=$mode
    if [ "$mode" = "cross" ] && [ -n "$target" ]; then
        target_line="$mode $target"
    fi

    return 0
}

common_build_validate_mode () {
    build_script=$1
    build_targets=$2

    if ! common_target_supported "$build_targets" "$target_line" \
            && ! common_target_supported "$build_targets" "$mode"; then
        echo "usage: $build_script <mode> [target]"
        printf '%s\n' "$build_targets"
        exit 1
    fi

    return 0
}

common_build_print_invocation () {
    project=$1

    if [ -n "${target:-}" ]; then
        printf '\n%s %s%s %s%s\n' \
            "$project" "$RED" "$mode" "$target" "$RES"
    else
        printf '\n%s %s%s%s\n' "$project" "$RED" "$mode" "$RES"
    fi

    return 0
}


common_build_unknown_mode () {
    if [ ! -f "${mode:-}" ]; then
        echo "Unknown mode $mode"
        exit 1
    fi

    return 0
}

common_build_cross_all () {
    cross_grep_out=${1:-}

    if [ "${target:-}" != "all" ]; then
        return 0
    fi

    cross_all_targets=$cross_targets
    if [ -n "$cross_grep_out" ]; then
        cross_all_targets=$(
            printf '%s\n' "$cross_targets" \
                | grep -v -- "$cross_grep_out" \
                || true
        )
    fi

    ncross=$(printf '%s\n' "$cross_all_targets" | wc -l)
    i=1

    for cross_target in $cross_all_targets; do
        echo "$i / $ncross"
        i=$((i+1))
        if ! "$0" cross "$cross_target" 2>&1 | head -n 200; then
            exit 1
        fi
    done

    exit 0
}

common_build_test_all () {
    if [ "$#" -lt 2 ]; then
        error "common_build_test_all <targets> <compiler> [compiler...]\n"
        exit 1
    fi

    test_all_targets=$1
    shift

    for build_target in $test_all_targets; do
        echo "target=$build_target"

        for compiler do
            printf '\nCC=%s%s%s\n' "$RED" "$compiler" "$RES"
            CC="$compiler" "$0" "$build_target" || exit 3
        done
    done

    for cross_target in $cross_targets; do
        echo "target=cross $cross_target"
        "$0" cross "$cross_target" || exit 3
    done

    exit 0
}

common_build_run_analyzers () {
    analyzer_mode=${1:-debug}
    analyzer_gcc_cflags=${2:-}

    if [ -z "$analyzer_gcc_cflags" ]; then
        analyzer_gcc_cflags="-fanalyzer -fdiagnostics-color=never"
    fi

    set +e

    CC=gcc CFLAGS="$analyzer_gcc_cflags" "$0" "$analyzer_mode"

    analyzer_clang_cflags="--analyze -Xanalyzer -analyzer-output=text"
    analyzer_clang_cflags="$analyzer_clang_cflags -Xanalyzer -analyzer-werror"
    analyzer_clang_cflags="$analyzer_clang_cflags -Xanalyzer"
    analyzer_clang_cflags="$analyzer_clang_cflags -analyzer-opt-analyze-headers"
    analyzer_clang_cflags="$analyzer_clang_cflags -Wno-unused-command-line-argument"
    analyzer_clang_cflags="$analyzer_clang_cflags -fno-color-diagnostics"
    CC=clang CFLAGS="$analyzer_clang_cflags" "$0" "$analyzer_mode"

    exit 0
}

common_build_incremental_roots_are_newer () {
    incremental_rebuild_target=$1
    incremental_rebuild_roots=${COMMON_BUILD_INCREMENTAL_ROOTS:-cbase}

    for incremental_rebuild_root in $incremental_rebuild_roots; do
        if [ -d "$incremental_rebuild_root" ] \
                && find "$incremental_rebuild_root" -type f \
                    -newer "$incremental_rebuild_target" \
                    -print | grep -q .; then
            return 0
        fi
    done

    return 1
}

common_build_incremental_binary () {
    if [ "$#" -ne 5 ]; then
        error "common_build_incremental_binary <exe> <source_root> "
        error "<main_source> <compile_flags> <link_flags>\n"
        exit 1
    fi

    incremental_exe=$1
    incremental_source_root=$2
    incremental_main_source=$3
    incremental_compile_flags=$4
    incremental_link_flags=$5

    incremental_mode=${mode:-debug}
    incremental_objdir=${COMMON_BUILD_INCREMENTAL_OBJDIR:-bin/obj/$incremental_mode}
    incremental_main_obj="$incremental_objdir/${incremental_main_source%.c}.o"
    incremental_flags_file=$incremental_objdir/flags
    incremental_subdir_sources=
    incremental_subdir_objects=

    for incremental_source_dir in "$incremental_source_root"/*; do
        if [ ! -d "$incremental_source_dir" ]; then
            continue
        fi

        incremental_source_subdir=${incremental_source_dir#$incremental_source_root/}
        incremental_wrapper_src=

        for incremental_source_file in "$incremental_source_dir"/*.c; do
            if [ ! -f "$incremental_source_file" ]; then
                continue
            fi

            if awk -v source_subdir="$incremental_source_subdir" '
                /^[[:space:]]*#[[:space:]]*include[[:space:]]*"/ {
                    include = $0
                    sub(/^[^"]*"/, "", include)
                    sub(/".*$/, "", include)

                    pattern = "^" source_subdir "/.*\\.c$"
                    if (include ~ pattern) {
                        found = 1
                    }
                }
                END { exit !found }
            ' "$incremental_source_file"; then
                if [ -n "$incremental_wrapper_src" ]; then
                    error "multiple incremental source files in %s\n" \
                        "$incremental_source_dir"
                    exit 1
                fi

                incremental_wrapper_src=$incremental_source_file
            fi
        done

        if [ -n "$incremental_wrapper_src" ]; then
            incremental_wrapper_obj=${incremental_wrapper_src%.c}.o
            incremental_wrapper_obj=$incremental_objdir/$incremental_wrapper_obj
            incremental_subdir_sources="$incremental_subdir_sources "
            incremental_subdir_sources="$incremental_subdir_sources$incremental_wrapper_src"
            incremental_subdir_objects="$incremental_subdir_objects "
            incremental_subdir_objects="$incremental_subdir_objects$incremental_wrapper_obj"
        fi
    done

    if [ -z "$incremental_subdir_sources" ]; then
        error "no incremental source files found under %s subfolders\n" \
            "$incremental_source_root"
        exit 1
    fi

    mkdir -p "$(dirname "$incremental_main_obj")"

    for incremental_subdir_object in $incremental_subdir_objects; do
        mkdir -p "$(dirname "$incremental_subdir_object")"
    done

    incremental_debug_flags="CC=$CC
SOURCE_ROOT=$incremental_source_root
MAIN_SOURCE=$incremental_main_source
COMPILE_FLAGS=$incremental_compile_flags
COMMON_ROOTS=${COMMON_BUILD_INCREMENTAL_ROOTS:-cbase}
SUBDIR_SOURCES=$incremental_subdir_sources"
    if [ ! -f "$incremental_flags_file" ] \
            || ! printf '%s\n' "$incremental_debug_flags" \
                | cmp -s - "$incremental_flags_file"; then
        rm -f "$incremental_main_obj"
        for incremental_subdir_object in $incremental_subdir_objects; do
            rm -f "$incremental_subdir_object"
        done

        printf '%s\n' "$incremental_debug_flags" > "$incremental_flags_file"
    fi

    incremental_compile_pids=

    for incremental_subdir_source in $incremental_subdir_sources; do
        incremental_subdir_object=${incremental_subdir_source%.c}.o
        incremental_subdir_object=$incremental_objdir/$incremental_subdir_object
        incremental_source_dir=$(dirname "$incremental_subdir_source")

        if [ ! -f "$incremental_subdir_object" ] \
                || find "$incremental_source_dir" -maxdepth 1 -type f \
                    \( -name '*.c' -o -name '*.h' \) \
                    -newer "$incremental_subdir_object" -print | grep -q . \
                || find "$incremental_source_root" -mindepth 2 \
                    -maxdepth 2 -type f \
                    -name '*.h' \
                    -newer "$incremental_subdir_object" -print | grep -q . \
                || find "$incremental_source_root" -maxdepth 1 \
                    -type f -name '*.h' \
                    -newer "$incremental_subdir_object" -print | grep -q . \
                || common_build_incremental_roots_are_newer \
                    "$incremental_subdir_object"; then
            (
                trace_on
                $CC \
                    $incremental_compile_flags \
                    -c "$incremental_subdir_source" \
                    -o "$incremental_subdir_object"
                trace_off
            ) &
            incremental_compile_pids="$incremental_compile_pids $!"
        fi
    done

    if [ ! -f "$incremental_main_obj" ] \
            || find "$incremental_source_root" -maxdepth 1 -type f \
                \( -name '*.c' -o -name '*.h' \) \
                -newer "$incremental_main_obj" -print | grep -q . \
            || find "$incremental_source_root" -mindepth 2 -maxdepth 2 \
                -type f -name '*.h' \
                -newer "$incremental_main_obj" -print | grep -q . \
            || common_build_incremental_roots_are_newer \
                "$incremental_main_obj"; then
        (
            trace_on
            $CC \
                $incremental_compile_flags \
                -DPROJECT_INCREMENTAL_BUILD=1 \
                -c "$incremental_main_source" \
                -o "$incremental_main_obj"
            trace_off
        ) &
        incremental_compile_pids="$incremental_compile_pids $!"
    fi

    incremental_compile_failed=0
    for incremental_compile_pid in $incremental_compile_pids; do
        if ! wait "$incremental_compile_pid"; then
            incremental_compile_failed=1
        fi
    done
    if [ "$incremental_compile_failed" != 0 ]; then
        exit 1
    fi

    trace_on
    $CC \
        -o "$incremental_exe" \
        "$incremental_main_obj" \
        $incremental_subdir_objects \
        $incremental_link_flags
    trace_off

    return 0
}

common_option_remove() {
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

common_gcc_flags_to_msvc() {
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
                if common_command_exists cygpath; then
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
            -lm|-lpthread|-pthread)
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


common_test_run_binary () {
    test_exe=$1

    if [ -n "${TEST_STDIN:-}" ]; then
        "$test_exe" < "$TEST_STDIN"
    else
        "$test_exe" < /dev/null
    fi
}

common_test_debugger () {
    test_exe=$1

    if common_command_exists gdb; then
        gdb --quiet \
            -ex run -ex backtrace -ex quit \
            "$test_exe" < /dev/null 2>&1 || true
    elif common_command_exists lldb; then
        lldb \
            --batch \
            --one-line "run" \
            --one-line "bt" \
            -- "$test_exe" < /dev/null 2>&1 || true
    fi

    return 0
}

common_test_source_is_excluded () {
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

common_test_executable_path () {
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

common_test_compile_and_run_source () {
    test_src=$1
    test_name=$(basename "$test_src")
    test_module=${test_name%.c}
    test_exe=$(common_test_executable_path "$test_module")
    test_cc=$CC
    test_cmd_flags="$CPPFLAGS $TEST_CPPFLAGS $CFLAGS $TEST_CFLAGS"
    test_added_flags=""
    test_tail_ldflags="$LDFLAGS"
    test_run_after_compile=1
    test_msvc_compiler=

    mkdir -p "$(dirname "$test_exe")"

    printf "\nTesting ${RED}%s${RES} ...\n" "$test_src"

    if [ -n "${TEST_WINDOWS_SOURCE_PATTERN:-}" ] \
            && echo "$test_src" | grep -Eq "$TEST_WINDOWS_SOURCE_PATTERN"; then
        if ! common_command_exists zig; then
            return 0
        fi

        test_cc="zig cc"
        test_cmdline="$test_cc $test_cmd_flags"
        test_cmdline=$(common_option_remove "$test_cmdline" "-D_GNU_SOURCE")
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
            test_cmd_flags=$(common_gcc_flags_to_msvc "$test_msvc_compiler" $test_cmd_flags)
            ;;
        cl|*/cl|cl.exe|*/cl.exe)
            test_msvc_compiler=cl
            test_cmd_flags=$(common_gcc_flags_to_msvc "$test_msvc_compiler" $test_cmd_flags)
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
        test_added_flags=$(common_gcc_flags_to_msvc "$test_msvc_compiler" $test_added_flags)
        test_tail_ldflags=$(common_gcc_flags_to_msvc "$test_msvc_compiler" $test_tail_ldflags)
    fi
    test_cmdline="$test_cmdline $test_added_flags"
    if [ "$test_msvc_compiler" = cl ]; then
        test_cmdline="$test_cmdline /Fe$test_exe $test_src"
    else
        test_cmdline="$test_cmdline -o $test_exe $test_src"
    fi
    test_cmdline="$test_cmdline $test_tail_ldflags"

    trace_on
    if $test_cmdline < /dev/null; then
        if [ "$test_run_after_compile" != 0 ] \
                && ! common_test_run_binary "$test_exe"; then
            common_test_debugger "$test_exe"
            exit 1
        fi
    else
        exit 1
    fi
    trace_off

    return 0
}

common_test () {
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

        if ! common_test_source_matches_filter "$test_src" "$TEST_FILTER"; then
            continue
        fi

        if common_test_source_is_excluded "$test_src"; then
            continue
        fi

        common_test_compile_and_run_source "$test_src"
    done

    return 0
}

common_build_tags () {
    if [ "$#" -eq 0 ]; then
        set -- .
    fi

    if common_command_exists ctags; then
        trace_on
        find "$@" -iname "*.[ch]" -print0 \
            | xargs --verbose -0 ctags --kinds-C=+l+d || true
        trace_off
    fi

    if [ -f tags ] && common_command_exists vtags.sed; then
        trace_on
        vtags.sed tags | sort | uniq > .tags.vim || true
        trace_off
    fi
}

common_install_file() {
    mode=$1
    src=$2
    dst=$3
    dst_dir=$(dirname "$dst")

    mkdir -p "$dst_dir"
    install -m "$mode" "$src" "$dst"
}

common_install_opt () {
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

common_uninstall_opt () {
    file="$1"
    dest="$2"

    if [ -e "$file" ]; then
        rm -rf "$dest"
    fi
}

common_compile_cbase () {
    CC="${CC:-cc}"

    trace_on
    $CC -g3 -O2 -c "cbase.c" -o "cbase.o"
    trace_off
}

if [ "$(basename "$0")" = "common.sh" ]; then
    common_compile_cbase
fi
