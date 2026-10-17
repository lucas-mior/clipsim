# shellcheck shell=sh

# Optional wall-clock profiling. Diagnostics remain on stdout/stderr as before;
# profiling records go to stderr and never enter the matched-pattern files.
common_profile_enabled=
case "${CBASE_FORBIDDEN_PROFILE:-}" in
''|0)
    ;;
*)
    common_profile_enabled=1
    ;;
esac

if [ -n "$common_profile_enabled" ]; then
    common_profile_now () {
        common_profile_timestamp=$(date +%s%N)
        case "$common_profile_timestamp" in
        *[!0-9]*|'')
            perl -MTime::HiRes=time -e \
                'printf "%.0f\n", time() * 1000000000'
            ;;
        *)
            printf '%s\n' "$common_profile_timestamp"
            ;;
        esac
    }

    common_profile_report () {
        common_profile_label=$1
        common_profile_end=$(common_profile_now)
        common_profile_elapsed=$((common_profile_end - common_profile_start))
        printf '[profile shell] %-22s %d.%03d ms\n' \
            "$common_profile_label" \
            "$((common_profile_elapsed / 1000000))" \
            "$((common_profile_elapsed % 1000000 / 1000))" >&2
        common_profile_start=$common_profile_end
    }

    common_profile_total_start=$(common_profile_now)
    common_profile_start=$common_profile_total_start
fi

common_libc_never=
common_libc_cbase_only=
common_libc_cbase_dir=
common_forbidden_patterns=
common_no_column_limit=
common_status_temporary_patterns=

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

if [ -f cbase/functions_forbidden_patterns.pl ]; then
    common_forbidden_patterns=cbase/functions_forbidden_patterns.pl
elif [ -f functions_forbidden_patterns.pl ]; then
    common_forbidden_patterns=functions_forbidden_patterns.pl
fi

if [ -f piscou.1 ] && [ -f config.h ]; then
    common_no_column_limit=config.h
fi

if [ -f cbase/status_temporary_patterns.py ]; then
    common_status_temporary_patterns=cbase/status_temporary_patterns.py
elif [ -f status_temporary_patterns.py ]; then
    common_status_temporary_patterns=status_temporary_patterns.py
else
    error "Error: status_temporary_patterns.py was not found\n"
    exit 1
fi

if [ -n "$common_libc_never" ] \
        || [ -n "$common_libc_cbase_only" ] \
        || [ -n "$common_forbidden_patterns" ] \
        || [ -n "$common_status_temporary_patterns" ]; then
    common_libc_pattern=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_sources=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_matches=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_matches2=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_matches3=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_matches4=$(mktemp "${TMPDIR:-/tmp}/common_libc.XXXXXX")
    common_libc_cbase_abs=

    : > "$common_libc_matches"
    : > "$common_libc_matches2"
    : > "$common_libc_matches3"
    : > "$common_libc_matches4"

    if [ -n "$common_libc_cbase_dir" ]; then
        common_libc_cbase_abs=$(
            readlink -f "$common_libc_cbase_dir" 2>/dev/null \
                || printf '%s\n' "$common_libc_cbase_dir"
        )
    fi

    {
        for common_libc_search_dir in \
                . cbase src test tests examples cyblocks tools; do
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

        # Vendored Ryu sources are part of the style checks, even when a
        # caller limits general discovery with TEST_MAXDEPTH.
        if [ -n "$common_libc_cbase_dir" ] \
                && [ -d "$common_libc_cbase_dir/ryu" ]; then
            find "$common_libc_cbase_dir/ryu" -type f \
                \( -iname "*.c" -o -iname "*.h" \)
        fi
    } | sort -u | while read -r common_source; do
        if echo "$common_source" | grep -Eq '(^|/)stc/'; then
            continue
        fi
        case "$common_source" in
        *.meta.h)
            continue
            ;;
        esac
        if [ -n "${TEST_EXCLUDE_PATTERN:-}" ] \
                && echo "$common_source" \
                    | grep -Eq "$TEST_EXCLUDE_PATTERN"; then
            continue
        fi
        if ! common_test_source_matches_filter \
                "$common_source" "${TEST_FILTER:-}"; then
            case "$common_source" in
            */ryu/*.c)
                # A 'ryu' test selection also checks the vendored C sources.
                case "${TEST_FILTER:-}" in
                ryu|ryu.c|*/ryu|*/ryu.c)
                    ;;
                *)
                    continue
                    ;;
                esac
                ;;
            *.c)
                continue
                ;;
            esac
        fi

        printf '%s\n' "$common_source"
    done > "$common_libc_sources"
    if [ -n "$common_profile_enabled" ]; then
        common_profile_report 'setup / source discovery'
        common_profile_file_count=$(wc -l < "$common_libc_sources")
        printf '[profile shell] selected sources: %s\n' \
            "$common_profile_file_count" >&2
    fi

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

    if [ -n "$common_profile_enabled" ]; then
        common_profile_report 'forbidden libc scan'
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

    if [ -n "$common_profile_enabled" ]; then
        common_profile_report 'cbase-only libc scan'
    fi

    if [ -n "$common_forbidden_patterns" ]; then
        perl "$common_forbidden_patterns" \
            --no-column-limit "$common_no_column_limit" \
            --file-list "$common_libc_sources" >> "$common_libc_matches3"
    fi

    if [ -n "$common_profile_enabled" ]; then
        common_profile_report 'Perl pattern checker'
    fi

    python3 "$common_status_temporary_patterns" \
        --file-list "$common_libc_sources" >> "$common_libc_matches4"

    if [ -n "$common_profile_enabled" ]; then
        common_profile_report 'Python status checker'
    fi

    if [ -s "$common_libc_matches" ]; then
        error "\nError: functions that must never be used:\n"
        cat "$common_libc_matches" >&2
    fi

    if [ -s "$common_libc_matches2" ]; then
        error "\nError: functions only allowed inside cbase:\n"
        cat "$common_libc_matches2" >&2
    fi

    if [ -s "$common_libc_matches3" ]; then
        error "\nError: forbidden code patterns and formatting idioms:\n"
        cat "$common_libc_matches3" >&2
    fi

    if [ -s "$common_libc_matches4" ]; then
        error "\nError: status temporaries whose assignment/use pairs should be inlined:\n"
        cat "$common_libc_matches4" >&2
    fi

    common_libc_status=0
    if [ -s "$common_libc_matches" ] \
            || [ -s "$common_libc_matches2" ] \
            || [ -s "$common_libc_matches3" ] \
            || [ -s "$common_libc_matches4" ]; then
        common_libc_status=1
        error "%s %s\n" \
            "In order to understand and learn how to fix the problems above," \
            "read cbase/c-guidelines.md and cbase/c-format.md"
    fi

    rm -f \
        "$common_libc_pattern" \
        "$common_libc_sources" \
        "$common_libc_matches" \
        "$common_libc_matches2" \
        "$common_libc_matches3" \
        "$common_libc_matches4"

    if [ -n "$common_profile_enabled" ]; then
        common_profile_report 'reporting / cleanup'
        common_profile_total_end=$(common_profile_now)
        common_profile_total=$((common_profile_total_end \
                                - common_profile_total_start))
        printf '[profile shell] %-22s %d.%03d ms\n' 'TOTAL' \
            "$((common_profile_total / 1000000))" \
            "$((common_profile_total % 1000000 / 1000))" >&2
    fi

    if [ "$common_libc_status" -ne 0 ]; then
        exit 1
    fi
fi
