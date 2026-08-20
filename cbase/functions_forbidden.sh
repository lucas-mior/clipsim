# shellcheck shell=sh

common_libc_never=
common_libc_cbase_only=
common_libc_cbase_dir=

common_test_source_matches_filter () {
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

if [ -f cbase/functions_never_use.txt ]; then
    common_libc_never=cbase/functions_never_use.txt
    common_libc_cbase_dir=cbase
elif [ -f functions_never_use.txt ]; then
    common_libc_never=functions_never_use.txt
    common_libc_cbase_dir=.
fi

if [ -f cbase/functions_allowed_cbase_only.txt ]; then
    common_libc_cbase_only=cbase/functions_allowed_cbase_only.txt
    common_libc_cbase_dir=cbase
elif [ -f functions_allowed_cbase_only.txt ]; then
    common_libc_cbase_only=functions_allowed_cbase_only.txt
    common_libc_cbase_dir=.
fi

if [ -n "$common_libc_never" ] \
        || [ -n "$common_libc_cbase_only" ]; then
    common_libc_pattern=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_sources=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_matches=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_matches2=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_cbase_abs=

    : > "$common_libc_matches"
    : > "$common_libc_matches2"

    if [ -n "$common_libc_cbase_dir" ]; then
        common_libc_cbase_abs=$(
            readlink -f "$common_libc_cbase_dir" 2>/dev/null \
                || printf '%s\n' "$common_libc_cbase_dir"
        )
    fi

    {
        for common_libc_search_dir in . cbase src test tests; do
            if [ ! -d "$common_libc_search_dir" ]; then
                continue
            fi

            if [ "$common_libc_search_dir" = . ]; then
                find "$common_libc_search_dir" -maxdepth 1 \
                    \( -iname "*.c" -o -iname "*.h" \)
            elif [ -n "${TEST_MAXDEPTH:-}" ]; then
                find "$common_libc_search_dir" -maxdepth "$TEST_MAXDEPTH" \
                    \( -iname "*.c" -o -iname "*.h" \)
            else
                find "$common_libc_search_dir" \
                    \( -iname "*.c" -o -iname "*.h" \)
            fi
        done
    } | sort -u | while read -r common_source; do
        if echo "$common_source" | grep -Eq '(^|/)stc/'; then
            continue
        fi
        if [ -n "${TEST_EXCLUDE_PATTERN:-}" ] \
                && echo "$common_source" \
                    | grep -Eq "$TEST_EXCLUDE_PATTERN"; then
            continue
        fi
        if ! common_test_source_matches_filter \
                "$common_source" "$TEST_FILTER"; then
            case "$common_source" in
            *.c)
                continue
                ;;
            esac
        fi

        printf '%s\n' "$common_source"
    done > "$common_libc_sources"

    if [ -n "$common_libc_never" ]; then
        awk '
            /^[[:space:]]*(#|$)/ { next }
            {
                printf "(^|[^[:alnum:]_])%s[[:space:]]*\\(\n", $1
            }
        ' "$common_libc_never" > "$common_libc_pattern"

        while read -r common_source; do
            grep -EnH -f "$common_libc_pattern" "$common_source" \
                >> "$common_libc_matches" || true
        done < "$common_libc_sources"
    fi

    if [ -n "$common_libc_cbase_only" ]; then
        awk '
            /^[[:space:]]*(#|$)/ { next }
            {
                printf "(^|[^[:alnum:]_])%s[[:space:]]*\\(\n", $1
            }
        ' "$common_libc_cbase_only" > "$common_libc_pattern"

        while read -r common_source; do
            common_source_abs=$(
                readlink -f "$common_source" 2>/dev/null \
                    || printf '%s\n' "$common_source"
            )

            case "$common_source_abs" in
            "$common_libc_cbase_abs"/*)
                continue
                ;;
            esac

            grep -EnH -f "$common_libc_pattern" "$common_source" \
                >> "$common_libc_matches2" || true
        done < "$common_libc_sources"
    fi

    if [ -s "$common_libc_matches" ]; then
        error "\nError: functions that must never be used:\n"
        cat "$common_libc_matches" >&2
    fi

    if [ -s "$common_libc_matches2" ]; then
        error "\nError: functions only allowed inside cbase:\n"
        cat "$common_libc_matches2" >&2
    fi

    common_libc_status=0
    if [ -s "$common_libc_matches" ] \
            || [ -s "$common_libc_matches2" ]; then
        common_libc_status=1
    fi

    rm -f \
        "$common_libc_pattern" \
        "$common_libc_sources" \
        "$common_libc_matches" \
        "$common_libc_matches2"

    if [ "$common_libc_status" -ne 0 ]; then
        exit 1
    fi
fi
