use strict;
use warnings;
use Cwd qw(abs_path);
use Digest::SHA;
use Encode qw(decode FB_CROAK);
use File::Basename qw(dirname);
use File::Path qw(make_path);
use File::Temp qw(tempfile);
use JSON::PP qw(decode_json encode_json);
use Time::HiRes qw(stat);

my $file_diagnostics = '';
my @paths = @ARGV;
my %no_column_limit;

if (@paths && $paths[0] eq '--no-column-limit') {
    my $path;

    shift @paths;
    $path = shift @paths;
    if (defined($path) && $path ne '') {
        $no_column_limit{abs_path($path) // $path} = 1;
    }
}

if (@paths && $paths[0] eq '--file-list') {
    my $list_path;
    my @listed_paths;

    shift @paths;
    $list_path = shift @paths;
    open my $list_fh, '<', $list_path or die "$list_path: $!\n";
    while (my $path = <$list_fh>) {
        chomp $path;
        if ($path ne '') {
            push @listed_paths, $path;
        }
    }
    @paths = @listed_paths;
}

# Cache diagnostics (including an empty result for clean files). Compare file
# metadata before reading: unchanged files are not opened again. The checker
# itself is hashed so changes to any rule invalidate all cached results.
sub file_signature {
    my ($path) = @_;
    my @info = stat($path);

    return undef unless @info;
    return join(':', @info[0, 1, 7],
                sprintf('%.9f', $info[9]), sprintf('%.9f', $info[10]));
}

sub checker_fingerprint {
    open my $fh, '<', $0 or die "$0: $!\n";
    binmode $fh;
    my $digest = Digest::SHA->new(256);
    $digest->addfile($fh);
    close $fh;
    return $digest->hexdigest;
}

sub read_cache {
    my ($path, $fingerprint) = @_;
    my $data;

    return {} unless open my $fh, '<', $path;
    local $/;
    $data = eval { decode_json(<$fh>) };
    close $fh;
    return {} unless ref($data) eq 'HASH'
                     && ($data->{version} // 0) == 2
                     && ($data->{checker} // '') eq $fingerprint
                     && ref($data->{files}) eq 'HASH';
    return $data->{files};
}

# Write atomically so interruptions never leave a partially written cache.
# Failure to create or update the cache must not break the checks.
sub write_cache {
    my ($path, $fingerprint, $files) = @_;
    my $dir = dirname($path);
    my ($fh, $temporary);

    eval { make_path($dir) unless -d $dir; };
    return if $@ || !-d $dir;
    eval {
        ($fh, $temporary) = tempfile('checker-XXXXXXXX', DIR => $dir,
                                      UNLINK => 0);
    };
    return if $@ || !$fh;
    my $data = encode_json({
        version => 2,
        checker => $fingerprint,
        files => $files,
    });
    my $written = print {$fh} $data;
    my $closed = close $fh;
    if (!$written || !$closed || !rename($temporary, $path)) {
        unlink $temporary;
    }
}

sub report_issue {
    my ($message) = @_;
    $file_diagnostics .= $message;
    print $message;
}

# These are purely textual formatting rules. Keep numerical and compiler-
# specific implementation idioms in Ryu untouched.
sub check_source_style {
    my ($path, $source, $column_limit, $is_ryu) = @_;
    my $line_number = 0;

    my @lines = split /\n/, $source, -1;
    pop @lines if length($source) && substr($source, -1) eq "\n";
    for my $line (@lines) {
        $line_number += 1;

        if (defined($column_limit)
                && length($line) > $column_limit
                && length(decode('UTF-8', $line, FB_CROAK))
                   > $column_limit) {
            report_issue("$path:$line_number:line exceeds "
                         . "$column_limit columns\n");
        }
        if ($line =~ /\t/) {
            report_issue("$path:$line_number:tab character is forbidden\n");
        }
        if ($line =~ /[ \t\r]+\z/) {
            report_issue("$path:$line_number:trailing whitespace\n");
        }
        if ($is_ryu && $line =~ /^\s*#\s*endif\b[^\n]*\/\//) {
            report_issue("$path:$line_number:use a block comment "
                         . "after #endif\n");
        }
    }
    if ($is_ryu && length($source) && substr($source, -1) ne "\n") {
        report_issue("$path:$line_number:missing final newline\n");
    }
}

# Comments and strings are masked before checking, so only actual
# preprocessor directives are reported. Apply this to all source files.
sub check_preprocessor_conditionals {
    my ($path, $code) = @_;
    my $line_number = 0;

    for my $line (split /\n/, $code) {
        $line_number += 1;
        if ($line =~ /^[ \t]*#[ \t]*(?:ifdef|ifndef)\b/) {
            report_issue("$path:$line_number:use #if defined or "
                         . "#if !defined\n");
        }
    }
}

# Use the signed SIZEOF wrapper in project code. Its own definition
# necessarily uses the built-in operator; suppress only that definition,
# including any preprocessor continuation lines.
sub check_builtin_sizeof {
    my ($path, $source, $code) = @_;
    my @source_lines = split /\n/, $source;
    my @code_lines = split /\n/, $code;
    my @sizeof_definition;
    my $in_definition = 0;

    for (my $i = 0; $i < @code_lines; $i += 1) {
        my $line = $code_lines[$i];

        if (!$in_definition
                && $line =~ /^[ \t]*#[ \t]*define[ \t]+SIZEOF\b/) {
            $in_definition = 1;
        }
        $sizeof_definition[$i] = $in_definition;
        $in_definition = $in_definition
                         && $source_lines[$i] =~ /\\[ \t]*\r?\z/;
    }

    pos($code) = 0;
    while ($code =~ /(?<![A-Za-z0-9_])sizeof(?![A-Za-z0-9_])
                    (?=\s*(?:\(|[A-Za-z_0-9*+!~&-]))/gx) {
        my $line = line_number($source, $-[0]);

        next if $sizeof_definition[$line - 1];
        report_issue("$path:$line:use SIZEOF instead of sizeof\n");
    }
}

# Report whitespace next to binary multiplication, not pointer declarators or
# unary dereferences. Keep this a conservative lexical check: C type names are
# recognized by the project's naming conventions and common C type spellings.
sub check_multiplication_spacing {
    my ($path, $source, $code) = @_;

    pos($code) = 0;
    while ($code =~ /\*/g) {
        my $idx = $-[0];
        my $left_idx = $idx - 1;
        my $right_idx = $idx + 1;
        my $left_char;
        my $right_char;

        next if substr($code, $idx + 1, 1) eq '=';
        next unless substr($source, $idx - 1, 1) =~ /[ \t]/
                    || substr($source, $idx + 1, 1) =~ /[ \t]/;

        while ($left_idx >= 0
               && substr($code, $left_idx, 1) =~ /[ \t\r]/) {
            $left_idx -= 1;
        }
        while ($right_idx < length($code)
               && substr($code, $right_idx, 1) =~ /\s/) {
            $right_idx += 1;
        }
        next if $left_idx < 0 || $right_idx >= length($code);
        $left_char = substr($code, $left_idx, 1);
        $right_char = substr($code, $right_idx, 1);
        next if $left_char eq "\n";

        # A binary * follows an expression and precedes another expression.
        # Pointer declarators and unary * generally do not meet this test.
        my $line_start = rindex($code, "\n", $idx - 1) + 1;
        my $line_prefix = substr($code, $line_start, $idx - $line_start);
        next if $line_prefix =~ /^\s*#\s*define\s+\w+\([^()\n]*\)\s*$/;
        if ($right_char =~ /[A-Za-z_]/
                && $line_prefix =~ /\bCAT\([^()\n]*\)\s*$/) {
            next;
        }

        next unless $left_char =~ /[A-Za-z0-9_\]\)}]/
                    && $right_char =~ /[A-Za-z0-9_(~!&*+\-]/;

        if ($left_char =~ /[A-Za-z0-9_]/) {
            my $word_end = $left_idx + 1;
            my $word_start = $left_idx;
            my $word;
            my $prefix_start;
            my $prefix;

            while ($word_start > 0
                   && substr($code, $word_start - 1, 1)
                      =~ /[A-Za-z0-9_]/) {
                $word_start -= 1;
            }
            $word = substr($code, $word_start, $word_end - $word_start);
            next if $word =~ /^(?:return|case|throw)$/;
            $prefix_start = rindex($code, "\n", $word_start - 1) + 1;
            $prefix = substr($code, $prefix_start,
                             $word_start - $prefix_start);
            $prefix =~ s/^.*[,(]//;
            if ($word =~ /^(?:const|restrict|volatile)$/) {
                $prefix =~ s/(?:\*\s*)+\z//;
            }

            # Types precede pointer names, but expressions can also have
            # identifiers on both sides of *. Do not exempt types when the
            # line already contains an expression operator or 'return'.
            if ($prefix !~ /[=+*\/%?:<>&|!^]/
                    && $prefix !~ /\breturn\b/
                    && ($word =~ /^(?:void|char|bool|float|double|signed
                                     |unsigned|short|int|long|int8|int16
                                     |int32|int64|uint8|uint16|uint32|uint64
                                     |uchar|llong|ullong|bool32|uint|ulong
                                     |const|restrict|volatile
                                     |size_t|ptrdiff_t|ssize_t|FILE|DIR
                                     |fftwf_complex)$/x
                        || $word =~ /^[A-Z][A-Za-z0-9_]*[a-z][A-Za-z0-9_]*$/
                        || ($word =~ /^(?:[A-Z][A-Z0-9_]*_TYPE
                                         |CONDITION_VARIABLE|FILETIME)$/x
                            && $right_char =~ /[a-z_]/)
                        || $word =~ /^[A-Za-z_][A-Za-z0-9_]*_t$/
                        || $prefix =~ /\b(?:struct|union|enum)\s+$/
                        || $word eq 'T')) {
                next;
            }
        }

        my $line = line_number($source, $idx);
        report_issue("$path:$line:space around multiplication * is forbidden\n");
    }
}

# Ternaries are prohibited by c-guidelines.md except in Ryu. Ignore macro
# definitions: metaprogramming helpers such as cbase/minmax.c use ?: on
# preprocessor lines, which are not ordinary C statements.
sub check_ternary_operators {
    my ($path, $code) = @_;
    my $line_number = 0;
    my $continuation = 0;

    for my $line (split /\n/, $code) {
        $line_number += 1;
        my $directive = $continuation || $line =~ /^[ \t]*#/;
        $continuation = $directive && $line =~ /\\[ \t]*\z/;
        next if $directive;
        if (index($line, '?') >= 0) {
            report_issue("$path:$line_number:ternary operator is forbidden\n");
        }
    }
}

my $cache_path = $ENV{CBASE_FORBIDDEN_PATTERNS_CACHE}
                 // '.cache/functions_forbidden_patterns.json';
my $checker_hash = checker_fingerprint();
my $cached_files = read_cache($cache_path, $checker_hash);
my $cache_dirty = 0;

# Count preceding newlines by binary search rather than rescanning every
# prefix.  Several diagnostics perform this lookup inside argument loops.
my @line_offsets;

sub line_number {
    my ($text, $idx) = @_;
    my ($left, $right) = (0, scalar @line_offsets);

    while ($left < $right) {
        my $middle = int(($left + $right) / 2);
        if ($line_offsets[$middle] < $idx) {
            $left = $middle + 1;
        } else {
            $right = $middle;
        }
    }
    return $left + 1;
}

sub mask_text {
    my ($text) = @_;
    $text =~ s/[^\n]/ /g;
    return $text;
}

sub code_mask {
    my ($source) = @_;
    my $code = $source;

    $code =~ s{
          "(?:\\.|[^"\\])*"
        | '(?:\\.|[^'\\])*'
        | //[^\n]*
        | /\*.*?\*/
    }{ mask_text($&) }gexs;

    return $code;
}

sub skip_space_comments {
    my ($text, $idx) = @_;

    while ($idx < length($text)) {
        my $ch = substr($text, $idx, 1);
        my $two = substr($text, $idx, 2);
        my $three = substr($text, $idx, 3);

        if ($three eq "\\\r\n") {
            $idx += 3;
        } elsif ($two eq "\\\n") {
            $idx += 2;
        } elsif ($ch =~ /\s/) {
            $idx += 1;
        } elsif ($two eq '//') {
            $idx += 2;
            while ($idx < length($text)
                   && substr($text, $idx, 1) ne "\n") {
                $idx += 1;
            }
        } elsif ($two eq '/*') {
            $idx += 2;
            while ($idx < length($text)
                   && substr($text, $idx, 2) ne '*/') {
                $idx += 1;
            }
            if ($idx < length($text)) {
                $idx += 2;
            }
        } else {
            last;
        }
    }

    return $idx;
}

sub call_starts_with_newline {
    my ($source, $start) = @_;
    pos($source) = $start;
    return $source =~ /\G[ \t]*\r?\n/gc;
}

sub call_end {
    my ($code, $args_start) = @_;
    my $idx = $args_start;
    my $depth = 1;

    while ($idx < length($code)) {
        my $ch = substr($code, $idx, 1);

        if ($ch eq '(') {
            $depth += 1;
        } elsif ($ch eq ')') {
            $depth -= 1;
            if ($depth == 0) {
                return $idx;
            }
        }

        $idx += 1;
    }

    return -1;
}

sub argument_ranges {
    my ($source, $code, $args_start, $args_end) = @_;
    my @arg_starts;
    my @arg_ends;
    my @arg_has_newline;
    my $first_arg_idx = skip_space_comments($source, $args_start);
    my $idx;
    my $paren_depth = 0;
    my $bracket_depth = 0;
    my $brace_depth = 0;

    if ($first_arg_idx >= $args_end
            || substr($source, $first_arg_idx, 1) eq ')') {
        return (\@arg_starts, \@arg_ends, \@arg_has_newline);
    }

    push @arg_starts, $first_arg_idx;
    push @arg_has_newline, 0;
    $idx = $args_start;
    while ($idx < $args_end) {
        my $ch = substr($code, $idx, 1);

        if ($ch eq '(') {
            $paren_depth += 1;
        } elsif ($ch eq ')') {
            $paren_depth -= 1;
        } elsif ($ch eq '[') {
            $bracket_depth += 1;
        } elsif ($ch eq ']') {
            $bracket_depth -= 1;
        } elsif ($ch eq '{') {
            $brace_depth += 1;
        } elsif ($ch eq '}') {
            $brace_depth -= 1;
        } elsif ($ch eq ',' && $paren_depth == 0
                 && $bracket_depth == 0 && $brace_depth == 0) {
            my $next_arg_idx = skip_space_comments($source, $idx + 1);

            if ($next_arg_idx < $args_end
                    && substr($source, $next_arg_idx, 1) ne ')') {
                push @arg_ends, $idx;
                push @arg_starts, $next_arg_idx;
                push @arg_has_newline,
                     substr($source, $idx + 1,
                            $next_arg_idx - $idx - 1) =~ /\n/ ? 1 : 0;
            }
        }

        $idx += 1;
    }
    push @arg_ends, $args_end;

    return (\@arg_starts, \@arg_ends, \@arg_has_newline);
}

sub strlit_arg_has_literal {
    my ($source, $arg_start) = @_;
    my $strlit_idx = skip_space_comments($source, $arg_start);
    my $paren_idx;
    my $literal_idx;

    if (substr($source, $strlit_idx, 6) ne 'STRLIT') {
        return 0;
    }

    $paren_idx = skip_space_comments($source, $strlit_idx + 6);
    if (substr($source, $paren_idx, 1) ne '(') {
        return 0;
    }

    $literal_idx = skip_space_comments($source, $paren_idx + 1);
    return substr($source, $literal_idx, 1) eq '"';
}

sub range_has_percent_string_literal {
    my ($source, $start, $end) = @_;
    my $idx = $start;

    while ($idx < $end) {
        my $ch = substr($source, $idx, 1);
        my $two = substr($source, $idx, 2);

        if ($two eq '//') {
            $idx += 2;
            while ($idx < $end && substr($source, $idx, 1) ne "\n") {
                $idx += 1;
            }
            next;
        }
        if ($two eq '/*') {
            $idx += 2;
            while ($idx < $end && substr($source, $idx, 2) ne '*/') {
                $idx += 1;
            }
            if ($idx < $end) {
                $idx += 2;
            }
            next;
        }
        if ($ch eq q{'}) {
            $idx += 1;
            while ($idx < $end) {
                $ch = substr($source, $idx, 1);
                if ($ch eq '\\') {
                    $idx += 2;
                    next;
                }
                $idx += 1;
                last if $ch eq q{'};
            }
            next;
        }
        if ($ch eq '"') {
            $idx += 1;
            while ($idx < $end) {
                $ch = substr($source, $idx, 1);
                if ($ch eq '\\') {
                    $idx += 2;
                    next;
                }
                if ($ch eq '%') {
                    return 1;
                }
                $idx += 1;
                last if $ch eq '"';
            }
            next;
        }

        $idx += 1;
    }

    return 0;
}

sub report_strequal_strlit_args {
    my ($path, $source, $code, $args_start, $args_end) = @_;
    my $args = substr($code, $args_start, $args_end - $args_start);
    my $arg_start = 0;
    my $idx = 0;
    my $depth = 0;

    while ($idx <= length($args)) {
        my $ch = substr($args, $idx, 1);
        my $arg_end = -1;

        if ($idx == length($args)) {
            $arg_end = $idx;
        } elsif ($ch eq '(') {
            $depth += 1;
        } elsif ($ch eq ')') {
            $depth -= 1;
        } elsif ($ch eq ',' && $depth == 0) {
            $arg_end = $idx;
        }

        if ($arg_end >= 0) {
            my $arg = substr($args, $arg_start, $arg_end - $arg_start);

            if ($arg =~ /^\s*STRLIT\s*\(/s
                    && strlit_arg_has_literal($source,
                                              $args_start + $arg_start)) {
                my $line = line_number($source, $args_start + $arg_start);
                report_issue("$path:$line:STREQUAL called with STRLIT literal\n");
                return;
            }

            $arg_start = $idx + 1;
        }

        $idx += 1;
    }

    return;
}

sub report_single_arg_format_string {
    my ($path, $source, $code, $args_start, $args_end) = @_;
    my $args = substr($code, $args_start, $args_end - $args_start);
    my $arg_start = 0;
    my $idx = 0;
    my $depth = 0;
    my $arg_count = 0;
    my $format_start = -1;
    my $format_end = -1;

    while ($idx <= length($args)) {
        my $ch = substr($args, $idx, 1);
        my $arg_end = -1;

        if ($idx == length($args)) {
            $arg_end = $idx;
        } elsif ($ch eq '(') {
            $depth += 1;
        } elsif ($ch eq ')') {
            $depth -= 1;
        } elsif ($ch eq ',' && $depth == 0) {
            $arg_end = $idx;
        }

        if ($arg_end >= 0) {
            $arg_count += 1;
            if ($arg_count == 2) {
                $format_start = $args_start + $arg_start;
                $format_end = $args_start + $arg_end;
                last;
            }

            $arg_start = $idx + 1;
        }

        $idx += 1;
    }

    if ($format_start < 0) {
        return;
    }

    my $format_arg = substr($source, $format_start,
                            $format_end - $format_start + 1);
    my $format_spec = qr{
        %[-+ \#0]*
        [0-9]*
        (?:\.(?:\*|[0-9]+))?
        (?:hh|h|ll|l|j|z|t|L)?
        [A-Za-z]
    }x;

    if ($format_arg =~ / "($format_spec)",/s) {
        my $line = line_number($source, $format_start);
        report_issue("$path:$line:single-argument format string "
                     . "without literal content\n");
    }

    return;
}

# Related consecutive arguments should share a line, except when each
# argument occupies its own line.
sub related_argument_kind {
    my ($first, $second) = @_;

    return '_len' if $second eq $first . '_len';

    if ($first =~ /\A([A-Za-z_][A-Za-z0-9_]*_)?(x|width)\z/) {
        my $prefix = defined($1) ? $1 : '';
        my $base = $2;
        my $partner = $base eq 'x' ? 'y' : 'height';

        if ($second eq $prefix . $partner) {
            return $base eq 'x' ? 'x/y' : 'width/height';
        }
    }

    return '';
}

# Identify comments without mistaking comment markers inside literals for
# actual comments.
sub range_has_comment {
    my ($source, $start, $end) = @_;
    my $text = substr($source, $start, $end - $start);

    while ($text =~ / "(?:\\.|[^"\\])*"
                    | '(?:\\.|[^'\\])*'
                    | (\/\/[^\n]* | \/\*.*?\*\/)
                  /gxs) {
        return 1 if defined $1;
    }
    return 0;
}

# A multiline string assembled from adjacent literals preserves explicit
# output line boundaries. Keep that layout when every fragment ends in \n.
sub has_split_newline_literals {
    my ($text) = @_;
    my $literal = qr/(?:u8|[uUL])?"(?:\\.|[^"\\])*"/;

    while ($text =~ /($literal(?:[ \t]*\r?\n[ \t]*$literal)+)/g) {
        my $run = $1;
        my @fragments = ($run =~ /$literal/g);
        my $all_end_in_newline = 1;

        for my $fragment (@fragments) {
            if ($fragment !~ /(?<!\\)(?:\\\\)*\\n"\z/) {
                $all_end_in_newline = 0;
                last;
            }
        }
        return 1 if $all_end_in_newline;
    }

    return 0;
}

# A short call can be wrapped deliberately to match a neighboring, longer
# call. Only reject unnecessary wrapping when every call in a consecutive
# (same-indentation, no intervening lines) group fits on one line.
sub report_unnecessary_call_wraps {
    my ($path, $source, $code, $column_limit) = @_;
    my @calls;
    my $previous_end = -1;

    return if $path =~ /\.meta\.h\z/;

    pos($code) = 0;
    while ($code =~ /(?<![A-Za-z0-9_])
                     ([A-Za-z_][A-Za-z0-9_]*)/gx) {
        my $name = $1;
        my $name_idx = $-[1];
        my $line_start;
        my $prefix;
        my $paren_idx;
        my $end_idx;
        my $line_end;
        my $tail;
        my $call_text;
        my $flat_text;
        my $fits;
        my $has_newline;
        my $start_line;
        my $end_line;
        my $indent;

        next if $name_idx < $previous_end;
        # These diagnostics intentionally keep the format string and values
        # together on the continuation line, even when the call would fit.
        next if $name eq 'error_impl' || $name eq 'assert_error';
        next if $name =~ /^(?:if|for|while|switch|sizeof|_Alignof
                            |_Generic|_Static_assert)$/x;
        $line_start = rindex($source, "\n", $name_idx - 1) + 1;
        $prefix = substr($code, $line_start, $name_idx - $line_start);
        next unless $prefix =~ /\A([ ]+)/;
        $indent = length($1);
        # Only inspect complete call statements and simple assignment or
        # return statements. In particular, a nested call or one following
        # an operator on a previous line is not an independent call.
        next unless $prefix =~ /\A[ ]+(?:return[ \t]+
                                    |[^(){};=\n]+(?<![=!<>])=[ \t]*
                                    )?\z/x;
        if ($prefix =~ /\A[ ]+\z/ && $line_start > 0) {
            my $prev_start = rindex($code, "\n", $line_start - 2) + 1;
            my $prev_line = substr($code, $prev_start,
                                   $line_start - $prev_start - 1);
            next if $prev_line =~ /(?:[=,(:?+\-*\/|&]|
                                      \breturn)[ \t]*\z/x;
        }

        $paren_idx = skip_space_comments($source, $name_idx + length($name));
        next unless substr($code, $paren_idx, 1) eq '(';
        $end_idx = call_end($code, $paren_idx + 1);
        next if $end_idx < 0;

        $line_end = index($source, "\n", $end_idx);
        $line_end = length($source) if $line_end < 0;
        $tail = substr($code, $end_idx + 1, $line_end - $end_idx - 1);
        next unless $tail =~ /\A[ \t]*;[ \t]*\r?\z/;

        # Retain only one complete call statement, not nested calls.
        $previous_end = $end_idx + 1;
        $call_text = substr($source, $line_start,
                            $line_end - $line_start);
        $has_newline = $call_text =~ /\n/ ? 1 : 0;
        $flat_text = $call_text;
        # Joining before a closing delimiter needs no intervening space.
        $flat_text =~ s/[ \t]*\r?\n[ \t]*(?=[)\],;])//g;
        $flat_text =~ s/[ \t]*\r?\n[ \t]*/ /g;
        $fits = !defined($column_limit)
                || length($flat_text) <= $column_limit;

        # These constructs require their original physical layout or are
        # not safely reducible to a single C source line.
        my $call_code = substr($code, $line_start,
                               $end_idx - $line_start);
        $fits = 0 if $call_text =~ /\\\r?\n|\n[ \t]*#/
                     || $call_code =~ /[{};]/
                     || range_has_comment($source, $line_start, $line_end)
                     || has_split_newline_literals($call_text);

        $start_line = line_number($source, $line_start);
        $end_line = line_number($source, $line_end);
        push @calls, {
            name => $name,
            start_line => $start_line,
            end_line => $end_line,
            indent => $indent,
            fits => $fits,
            wrapped => $has_newline,
        };
    }

    # A group is a maximal sequence of complete call statements at the
    # same indentation, with no other code or blank line between them.
    for (my $start = 0; $start < @calls;) {
        my $end = $start + 1;
        my $all_fit = $calls[$start]{fits};

        while ($end < @calls
               && $calls[$end]{start_line}
                  == $calls[$end - 1]{end_line} + 1
               && $calls[$end]{indent} == $calls[$start]{indent}) {
            $all_fit &&= $calls[$end]{fits};
            $end += 1;
        }

        if ($all_fit) {
            for (my $idx = $start; $idx < $end; $idx += 1) {
                next unless $calls[$idx]{wrapped};
                report_issue("$path:$calls[$idx]{start_line}:"
                             . "$calls[$idx]{name} call fits on one line "
                             . "but is split across multiple lines\n");
            }
        }
        $start = $end;
    }

    return;
}

# Put the assignment operator at the end of the preceding line rather
# than at the start of a continuation line. Comparison operators such as ==
# may begin a line; preprocessor continuations are outside this style rule.
sub check_assignment_equals_at_line_start {
    my ($path, $source, $code) = @_;
    my @lines = split /\n/, $source;
    my @code_lines = split /\n/, $code;
    my $continuation = 0;

    for (my $i = 0; $i < @lines; $i += 1) {
        my $line = $lines[$i];
        my $masked = $code_lines[$i];
        my $directive = $continuation || $masked =~ /^[ \t]*#/;

        $continuation = $directive && $line =~ /\\[ \t]*\r?\z/;
        next if $directive;
        if ($masked =~ /^[ \t]*=(?!=)/) {
            report_issue("$path:" . ($i + 1)
                         . ":assignment = must end the preceding line "
                         . "instead of beginning a line\n");
        }
    }
}

# A short assignment broken immediately after '=' may match a related,
# longer assignment. Inspect complete assignments (including unwrapped ones)
# and report short wraps only if every assignment in the consecutive,
# same-indentation block fits on one line. Blank lines separate blocks.
sub check_unnecessary_assignment_wraps {
    my ($path, $source, $code, $column_limit) = @_;
    my @lines = split /\n/, $source;
    my @code_lines = split /\n/, $code;
    my @assignments;
    my $continuation = 0;

    for (my $i = 0; $i < @lines;) {
        my $line = $lines[$i];
        my $masked = $code_lines[$i];
        my $directive = $continuation || $masked =~ /^[ \t]*#/;
        my $indent;
        my $left;
        my $wrapped;
        my $flat;
        my $end;
        my $complete = 0;

        $continuation = $directive && $line =~ /\\[ \t]*\r?\z/;
        if ($directive
            || $masked !~ /^([ \t]*)(\S.*?)(?<![=<>!+*\/\%&|^-])=(?!=)/) {
            $i += 1;
            next;
        }
        $indent = length($1);
        $left = $2;
        # Only a standalone assignment can participate in this grouping.
        # Ignore assignments embedded in conditions or other statements.
        if ($left =~ /[;{}]/
            || $left =~ /\A(?:if|for|while|switch|return|case|else)\b/) {
            $i += 1;
            next;
        }
        $wrapped = $line =~ /(?<![=<>!+*\/\%&|^-])=[ \t]*\r?\z/
                   && $masked =~ /(?<![=<>!+*\/\%&|^-])=[ \t]*\r?\z/;
        $flat = $line;
        $flat =~ s/[ \t\r]+\z//;
        $end = $i;

        while ($end < @lines) {
            my $part_code = $code_lines[$end];

            if ($part_code =~ /;[ \t]*\r?\z/
                && $part_code !~ /;.*;/) {
                $complete = 1;
                last;
            }
            $end += 1;
            last if $end >= @lines;
            my $part = $lines[$end];
            $part_code = $code_lines[$end];

            # A joined assignment must be a single complete statement.
            last if $part !~ /\S/ || $part =~ /\\[ \t]*\r?\z/
                    || $part_code =~ /^[ \t]*#/ || $part_code =~ /[{}]/;
            my $trimmed = $part;
            $trimmed =~ s/^[ \t]+//;
            $trimmed =~ s/[ \t\r]+\z//;
            $flat .= ' ' . $trimmed;
            last if $part_code =~ /;.*\S/ && $part_code !~ /;[ \t]*\r?\z/;
        }

        if (!$complete) {
            $i += 1;
            next;
        }
        my $original = join("\n", @lines[$i .. $end]);
        my $statement_code = join("\n", @code_lines[$i .. $end]);
        my $fits = (!defined($column_limit)
                    || length($flat) <= $column_limit)
                   && $original !~ /\\\r?\n|\n[ \t]*#/
                   && $statement_code !~ /[{}]/
                   && !range_has_comment($original, 0, length($original))
                   && !has_split_newline_literals($original);
        push @assignments, {
            start_line => $i + 1,
            end_line => $end + 1,
            indent => $indent,
            fits => $fits,
            wrapped => $wrapped,
        };
        $i = $end + 1;
    }

    for (my $start = 0; $start < @assignments;) {
        my $end = $start + 1;
        my $all_fit = $assignments[$start]{fits};

        while ($end < @assignments
               && $assignments[$end]{indent} == $assignments[$start]{indent}) {
            my $previous = $assignments[$end - 1]{end_line};
            my $next = $assignments[$end]{start_line};
            my $adjacent = 1;

            # Permit comment-only lines between assignments, but never
            # bridge a blank line or an intervening non-assignment statement.
            for (my $line = $previous; $line < $next - 1; $line += 1) {
                if ($lines[$line] !~ /\S/ || $code_lines[$line] =~ /\S/) {
                    $adjacent = 0;
                    last;
                }
            }
            last unless $adjacent;
            $all_fit &&= $assignments[$end]{fits};
            $end += 1;
        }

        if ($all_fit) {
            for (my $idx = $start; $idx < $end; $idx += 1) {
                next unless $assignments[$idx]{wrapped};
                report_issue("$path:$assignments[$idx]{start_line}:"
                             . "assignment fits on one line "
                             . "but is split after =\n");
            }
        }
        $start = $end;
    }
}

# Adjacent lower- and upper-bound checks on the same variable should use
# the inclusive ASSERT_BETWEEN form. Parse calls instead of matching whole
# argument lists with a regular expression, so nested size expressions work.
sub check_consecutive_bounds_assertions {
    my ($path, $source, $code) = @_;
    my $variable = qr{
        [A-Za-z_][A-Za-z0-9_]*
        (?:\s*(?:\.|->)\s*[A-Za-z_][A-Za-z0-9_]*
          |\s*\[\s*[A-Za-z_0-9]+\s*\])*
    }x;

    pos($code) = 0;
    while ($code =~ /(?<![A-Za-z0-9_])ASSERT_GE(?![A-Za-z0-9_])/g) {
        my $start = $-[0];
        my $paren = skip_space_comments($source, $+[0]);
        my $end;
        my ($starts, $ends);
        my ($value, $lower);
        my $following;
        my $next_paren;
        my $next_end;
        my ($next_starts, $next_ends);
        my ($next_value, $upper);
        my $line_start = rindex($code, "\n", $start - 1) + 1;
        my $prefix = substr($code, $line_start, $start - $line_start);

        next if $prefix =~ /^\s*#/;
        next unless substr($code, $paren, 1) eq '(';
        $end = call_end($code, $paren + 1);
        next if $end < 0;
        ($starts, $ends) = argument_ranges($source, $code,
                                           $paren + 1, $end);
        next unless @$starts == 2;
        $value = substr($code, $starts->[0], $ends->[0] - $starts->[0]);
        $lower = substr($code, $starts->[1], $ends->[1] - $starts->[1]);
        $value =~ s/\s+//g;
        $lower =~ s/\s+//g;
        next unless $value =~ /\A$variable\z/ && $lower eq '0';

        # Exactly the next physical line, with no intervening statement or
        # blank line. Comments are masked without changing line positions.
        $following = substr($code, $end + 1);
        next unless $following =~ /\A[ \t]*;[ \t]*(?:\\[ \t]*)?\r?\n
                                  [ \t]*ASSERT_LT(?![A-Za-z0-9_])/x;
        $next_paren = skip_space_comments($source, $end + 1 + $+[0]);
        next unless substr($code, $next_paren, 1) eq '(';
        $next_end = call_end($code, $next_paren + 1);
        next if $next_end < 0;
        ($next_starts, $next_ends) = argument_ranges(
            $source, $code, $next_paren + 1, $next_end);
        next unless @$next_starts == 2;
        $next_value = substr($code, $next_starts->[0],
                             $next_ends->[0] - $next_starts->[0]);
        $upper = substr($code, $next_starts->[1],
                        $next_ends->[1] - $next_starts->[1]);
        $next_value =~ s/\s+//g;
        $upper =~ s/^\s+|\s+$//g;
        next unless $next_value eq $value && length($upper);
        next unless substr($code, $next_end + 1) =~ /\A[ \t]*;/;

        my $line = line_number($source, $start);
        report_issue("$path:$line:use ASSERT_BETWEEN($value, 0, "
                     . "$upper - 1) instead of consecutive "
                     . "ASSERT_GE/ASSERT_LT\n");
    }
}

# An extra scope-only block at three indentation levels obscures the
# surrounding control flow. Inspect brace context rather than matching a
# bare opening brace: initializers and braces belonging to declarations or
# control-flow constructs are not artificial blocks.
sub check_deep_artificial_blocks {
    my ($path, $source, $code) = @_;
    my @source_lines = split /\n/, $source;
    my @code_lines = split /\n/, $code;
    my @initializer_stack;
    my $previous_code = '';
    my $continuation = 0;

    for (my $i = 0; $i < @code_lines; $i += 1) {
        my $line = $code_lines[$i];
        my $original = $source_lines[$i];
        my $directive = $continuation || $line =~ /^[ \t]*#/;

        $continuation = $directive && $original =~ /\\[ \t]*\r?\z/;
        next if $directive;

        while ($line =~ /[{}]/g) {
            my $brace = $&;
            my $column = $-[0];

            if ($brace eq '}') {
                pop @initializer_stack if @initializer_stack;
                next;
            }

            my $prefix = substr($line, 0, $column);
            my $context = $prefix =~ /\S/ ? $prefix : $previous_code;
            my $initializer = @initializer_stack
                              && $initializer_stack[-1];
            my $has_header;

            $context =~ s/\s+\z//;
            $initializer ||= $context =~ /[=,(]\z/;
            $has_header = $context =~ /\)\z/
                          || $context =~ /\b(?:else|do)\z/
                          || $context =~ /\b(?:struct|union|enum)
                                         (?:\s+[A-Za-z_][A-Za-z0-9_]*)?
                                         \z/x
                          || $context =~ /\b(?:case\b[^;{}]*|default):\z/;

            if (!$initializer && !$has_header && $column >= 12
                    && $original =~ /\A {12,}/) {
                my $line_number = $i + 1;
                report_issue("$path:$line_number:artificial block at "
                             . "nesting level 3 or more is forbidden\n");
            }
            push @initializer_stack, $initializer ? 1 : 0;
        }

        if ($line =~ /\S/) {
            $previous_code = $line;
            $previous_code =~ s/\s+\z//;
        }
    }
}

for my $path (@paths) {
    next if $path =~ /\.meta\.h\z/;

    my $absolute_path = abs_path($path) // $path;

    next if $path =~ m{(?:\A|/)tests/fixtures/}
            || $absolute_path =~ m{(?:\A|/)tests/fixtures/};

    my $column_limit = $no_column_limit{$absolute_path}
                       ? undef
                       : $absolute_path =~ m{(?:\A|/)cecup/src/}
                         ? 100 : 80;

    # The displayed path is part of the diagnostic, so distinguish spellings
    # such as "file.c" and "./file.c" even if both resolve to the same file.
    my $key = join("\0", $path, $absolute_path,
                   defined($column_limit) ? $column_limit : 'none');
    my $before = file_signature($path);
    my $entry = $cached_files->{$key};

    if (defined($before) && ref($entry) eq 'HASH'
            && defined($entry->{signature})
            && !ref($entry->{signature})
            && $entry->{signature} eq $before
            && defined($entry->{diagnostics})
            && !ref($entry->{diagnostics})) {
        print $entry->{diagnostics};
        next;
    }

    $file_diagnostics = '';
    open my $fh, '<', $path or die "$path: $!\n";
    local $/;
    my $source = <$fh>;
    close $fh;
    @line_offsets = ();
    while ($source =~ /\n/g) {
        push @line_offsets, $-[0];
    }
    my $code = code_mask($source);
    my $is_ryu = $absolute_path =~ m{(?:\A|/)cbase/ryu/[^/]+\.[ch]\z};

    check_preprocessor_conditionals($path, $code);
    check_multiplication_spacing($path, $source, $code) unless $is_ryu;
    check_builtin_sizeof($path, $source, $code) unless $is_ryu;
    check_consecutive_bounds_assertions($path, $source, $code)
        unless $is_ryu;
    check_deep_artificial_blocks($path, $source, $code)
        unless $is_ryu || $path =~ /\.meta\.h\z/;

    check_source_style($path, $source, $column_limit, $is_ryu);

    if (!$is_ryu) {
        # check_ternary_operators($path, $code);
    }

    # Indentation distinguishes calls from the project's function headers.
    # Generated .meta.h files may break before the first argument.
    # Scan each indented line once rather than repeatedly searching for
    # a call name at the end of the line.
    while ($path !~ /\.meta\.h\z/
           && $code =~ /^([ \t]+[^\n]*)/gm) {
        my $idx = $-[1];
        my $text = $1;

        next unless $text =~ /(?<![A-Za-z0-9_])
                             ([A-Za-z_][A-Za-z0-9_]*)[ \t]*(\()
                             [ \t]*\r?\z/x;

        my $name = $1;
        my $paren_idx = $idx + $-[2];
        my $prefix = substr($text, 0, $-[2]);
        my $arg_idx = skip_space_comments($source, $paren_idx + 1);

        # Masked comments must not count as indentation or hide directives.
        if (substr($source, $idx, 1) !~ /[ \t]/
                || $prefix =~ /^[ \t]*#/
                || $name =~ /^(?:if|for|while|switch|sizeof|_Alignof
                                |_Generic|_Static_assert)$/x) {
            next;
        }

        # Inspect the source so masked comments and literals still count.
        if ($arg_idx >= length($source)
                || substr($source, $arg_idx, 1) eq ')'
                || !call_starts_with_newline($source, $paren_idx + 1)) {
            next;
        }

        my $line = line_number($source, $paren_idx);
        report_issue("$path:$line:$name call breaks before the first "
                     . "argument\n");
    }

    if ($path !~ /\.meta\.h\z/) {
        pos($code) = 0;
        while ($code =~ /^([A-Za-z_][A-Za-z0-9_]*)[ \t]*(\()/gm) {
            my $name = $1;
            my $name_idx = $-[1];
            my $paren_idx = $-[2];
            my $line_start = $name_idx;
            my $prev_end;
            my $prev_start;
            my $prev_line;
            my $args_start;
            my $args_end;
            my $next_idx;
            my $arg_starts;
            my $arg_ends;
            my $arg_has_newline;
            my $anchor_idx;
            my $anchor_line_start = 0;
            my $anchor_column;

            next if $name =~ /^(?:if|for|while|switch|sizeof|_Alignof
                                |_Generic|_Static_assert)$/x;
            next if $line_start == 0;

            $prev_end = $line_start - 1;
            $prev_start = rindex($source, "\n", $prev_end - 1) + 1;
            $prev_line = substr($source, $prev_start,
                                $prev_end - $prev_start);
            $prev_line =~ s/\r\z//;

            # Project function declarations and definitions put the return
            # type on the previous, unindented line. This keeps column-zero
            # macro invocations out of the function-header checks.
            next unless $prev_line =~ /\S/;
            next if $prev_line =~ /^[ \t]|^[ \t]*#/;
            next if $prev_line =~ /[;{},)]\s*\z/;
            next if $prev_line =~ /\(/;

            $args_start = $paren_idx + 1;
            $args_end = call_end($code, $args_start);
            next if $args_end < 0;
            next unless substr($source, $line_start,
                               $args_end - $line_start) =~ /\n/;

            $next_idx = skip_space_comments($source, $args_end + 1);
            next unless substr($source, $next_idx, 1) =~ /[;{]/;

            ($arg_starts, $arg_ends, $arg_has_newline) =
                argument_ranges($source, $code, $args_start, $args_end);
            next if scalar(@$arg_starts) < 2;

            $anchor_idx = $arg_starts->[0];
            if ($anchor_idx > 0) {
                $anchor_line_start = rindex($source, "\n",
                                            $anchor_idx - 1) + 1;
            }
            $anchor_column = $anchor_idx - $anchor_line_start;

            for (my $arg_idx = 1;
                 $arg_idx < scalar(@$arg_starts); $arg_idx += 1) {
                my $current_idx;
                my $current_line_start = 0;
                my $current_column;
                my $line;

                next unless $arg_has_newline->[$arg_idx];
                $current_idx = $arg_starts->[$arg_idx];
                if ($current_idx > 0) {
                    $current_line_start = rindex($source, "\n",
                                                 $current_idx - 1) + 1;
                }
                $current_column = $current_idx - $current_line_start;
                next if $current_column == $anchor_column;

                $line = line_number($source, $current_idx);
                report_issue("$path:$line:$name function header parameter "
                             . "is not aligned with first parameter\n");
                last;
            }
        }
    }

    if ($path !~ /\.meta\.h\z/) {
        pos($code) = 0;
        while ($code =~ /(?<![A-Za-z0-9_])
                         ([A-Za-z_][A-Za-z0-9_]*)/gx) {
            my $name = $1;
            my $name_idx = $-[1];
            my $line_start = 0;
            my $line_prefix;
            my $paren_idx;
            my $args_start;
            my $args_end;
            my $first_arg_idx;
            my $arg_starts;
            my $arg_ends;
            my $arg_has_newline;
            my $alignment_arg = 0;
            my $format_arg_idx = -1;
            my $anchor_idx;
            my $anchor_line_start;
            my $anchor_column;

            if ($name_idx > 0) {
                $line_start = rindex($source, "\n", $name_idx - 1) + 1;
            }
            $line_prefix = substr($source, $line_start,
                                  $name_idx - $line_start);

            # Function definitions and declarations start at column zero.
            # Calls in expressions are indented, including calls after
            # return, assignments, and control-flow keywords.
            next unless $line_prefix =~ /^[ \t]/;
            next if $line_prefix =~ /^[ \t]*#/;
            next if $name =~ /^(?:if|for|while|switch|sizeof|_Alignof
                                |_Generic|_Static_assert)$/x;

            $paren_idx = skip_space_comments($source,
                                              $name_idx + length($name));
            next unless substr($source, $paren_idx, 1) eq '(';

            $args_start = $paren_idx + 1;
            $args_end = call_end($code, $args_start);
            next if $args_end < 0;

            $first_arg_idx = skip_space_comments($source, $args_start);
            next if $first_arg_idx >= $args_end
                    || substr($source, $first_arg_idx, 1) eq ')';

            ($arg_starts, $arg_ends, $arg_has_newline) =
                argument_ranges($source, $code, $args_start, $args_end);

            for (my $arg_idx = 0;
                 $arg_idx < scalar(@$arg_starts); $arg_idx += 1) {
                my $arg = substr($source, $arg_starts->[$arg_idx],
                                 $arg_ends->[$arg_idx]
                                 - $arg_starts->[$arg_idx]);
                my $literal = qr/(?:u8|[uUL])?"(?:\\.|[^"\\])*"/;
                my $first_idx = $arg_starts->[$arg_idx];
                my $first_line_start;
                my $first_column;

                next unless $arg =~ /\A$literal/;
                $first_line_start = rindex($source, "\n", $first_idx - 1)
                                    + 1;
                $first_column = $first_idx - $first_line_start;
                while ($arg =~ /$literal[ \t]*\r?\n[ \t]*
                               (?=($literal))/gx) {
                    my $second_idx = $arg_starts->[$arg_idx] + $-[1];
                    my $second_line_start = rindex($source, "\n",
                                                   $second_idx - 1) + 1;

                    next if $first_column
                            == $second_idx - $second_line_start;

                    my $line = line_number($source, $second_idx);
                    report_issue("$path:$line:$name call string literal is "
                                 . "not aligned with first literal\n");
                    last;
                }
            }

            # printf-like calls align wrapped arguments with the format
            # string. Detect that argument by looking for a '%' inside a
            # string literal, so local wrappers follow the same rule.
            for (my $arg_idx = 0;
                 $arg_idx < scalar(@$arg_starts); $arg_idx += 1) {
                if (range_has_percent_string_literal($source,
                                                     $arg_starts->[$arg_idx],
                                                     $arg_ends->[$arg_idx])) {
                    $format_arg_idx = $arg_idx;
                    last;
                }
            }

            if ($format_arg_idx >= 0
                    && $format_arg_idx < scalar(@$arg_starts)) {
                $alignment_arg = $format_arg_idx;
            }

            if ($alignment_arg >= scalar(@$arg_starts)) {
                $alignment_arg = 0;
            }
            $anchor_idx = $arg_starts->[$alignment_arg];
            $anchor_line_start = 0;
            if ($anchor_idx > 0) {
                $anchor_line_start = rindex($source, "\n", $anchor_idx - 1)
                                     + 1;
            }
            $anchor_column = $anchor_idx - $anchor_line_start;

            for (my $arg_idx = $alignment_arg + 1;
                 $arg_idx < scalar(@$arg_starts); $arg_idx += 1) {
                my $current_idx;
                my $current_line_start = 0;
                my $current_column;
                my $line;

                next unless $arg_has_newline->[$arg_idx];
                $current_idx = $arg_starts->[$arg_idx];
                if ($current_idx > 0) {
                    $current_line_start = rindex($source, "\n",
                                                 $current_idx - 1) + 1;
                }
                $current_column = $current_idx - $current_line_start;
                next if $current_column == $anchor_column;

                $line = line_number($source, $current_idx);
                report_issue("$path:$line:$name call argument is not aligned "
                             . "with its alignment anchor\n");
                last;
            }

            if ($format_arg_idx >= 0
                    && $format_arg_idx < scalar(@$arg_starts)
                    && $format_arg_idx + 1 < scalar(@$arg_starts)) {
                my $format_line = line_number(
                    $source, $arg_ends->[$format_arg_idx]);
                my $format_line_has_args = 0;
                my $other_line_has_args = 0;

                for (my $arg_idx = $format_arg_idx + 1;
                     $arg_idx < scalar(@$arg_starts); $arg_idx += 1) {
                    my $line = line_number($source,
                                           $arg_starts->[$arg_idx]);

                    if ($line == $format_line) {
                        $format_line_has_args = 1;
                    } else {
                        $other_line_has_args = 1;
                    }
                }

                if ($format_line_has_args && $other_line_has_args) {
                    report_issue("$path:$format_line:$name format-string "
                                 . "line must contain all format arguments "
                                 . "or none\n");
                }
            }

            if (scalar(@$arg_starts) >= 3) {
                my @arg_names;
                my @arg_lines;
                my %args_per_line;

                for (my $arg_idx = 0;
                     $arg_idx < scalar(@$arg_starts); $arg_idx += 1) {
                    my $arg = substr($code, $arg_starts->[$arg_idx],
                                     $arg_ends->[$arg_idx]
                                     - $arg_starts->[$arg_idx]);
                    my $line = line_number($source,
                                           $arg_starts->[$arg_idx]);

                    if ($arg =~ /^\s*([A-Za-z_][A-Za-z0-9_]*)\s*$/s) {
                        $arg_names[$arg_idx] = $1;
                    }
                    $arg_lines[$arg_idx] = $line;
                    $args_per_line{$line} += 1;
                }

                for (my $arg_idx = 0;
                     $arg_idx + 1 < scalar(@$arg_starts); $arg_idx += 1) {
                    my $first_line;
                    my $second_line;
                    my $kind;

                    next unless defined $arg_names[$arg_idx]
                            && defined $arg_names[$arg_idx + 1];
                    $kind = related_argument_kind($arg_names[$arg_idx],
                                                  $arg_names[$arg_idx + 1]);
                    next unless $kind;

                    $first_line = $arg_lines[$arg_idx];
                    $second_line = $arg_lines[$arg_idx + 1];
                    next if $first_line == $second_line;
                    next if $args_per_line{$first_line} == 1
                            && $args_per_line{$second_line} == 1;

                    report_issue("$path:$second_line:$name related $kind "
                                 . "arguments must be the only arguments on "
                                 . "their lines\n");
                    last;
                }
            }
        }
    }

    check_assignment_equals_at_line_start($path, $source, $code)
        unless $is_ryu || $path =~ /\.meta\.h\z/;

    check_unnecessary_assignment_wraps($path, $source, $code,
                                       $column_limit)
        unless $is_ryu || $path =~ /\.meta\.h\z/;

    report_unnecessary_call_wraps($path, $source, $code, $column_limit);

    pos($code) = 0;
    while ($code =~ /(?<![A-Za-z0-9_])ASSERT(?![A-Za-z0-9_])
                     \s*\(\s*[a-z_]+\s*(>=|>)\s*[0-9]+\s*\)\s*;/gx) {
        my $assertion = $1 eq '>=' ? 'ASSERT_GE' : 'ASSERT_GT';
        my $line = line_number($source, $-[0]);

        report_issue("$path:$line:use $assertion"
                     . " for comparison with integers\n");
    }

    pos($code) = 0;
    while ($code =~ /(?<![A-Za-z0-9_])ASSERT(?![A-Za-z0-9_])
                     \s*\(\s*[a-z_]+\s*(<=|<)\s*[0-9]+\s*\)\s*;/gx) {
        my $assertion = $1 eq '<=' ? 'ASSERT_LE' : 'ASSERT_LT';
        my $line = line_number($source, $-[0]);

        report_issue("$path:$line:use $assertion"
                     . " for comparison with integers\n");
    }

    pos($code) = 0;
    while ($code =~ /(?<![A-Za-z0-9_])STRLIT_LEN(?![A-Za-z0-9_])/g) {
        my $idx = $-[0];
        my $paren_idx = skip_space_comments($source, $idx + 10);
        my $literal_idx;

        if (substr($source, $paren_idx, 1) ne '(') {
            next;
        }

        $literal_idx = skip_space_comments($source, $paren_idx + 1);
        if (substr($source, $literal_idx, 1) eq '"') {
            my $line = line_number($source, $idx);
            report_issue("$path:$line:STRLIT_LEN called on a literal string\n");
        }
    }

    pos($code) = 0;
    while ($code =~ /(?<![A-Za-z0-9_])STREQUAL(?![A-Za-z0-9_])/g) {
        my $paren_idx = skip_space_comments($source, $-[0] + 8);
        my $args_start;
        my $args_end;

        if (substr($source, $paren_idx, 1) ne '(') {
            next;
        }

        $args_start = $paren_idx + 1;
        $args_end = call_end($code, $args_start);
        if ($args_end >= 0) {
            report_strequal_strlit_args($path, $source, $code,
                                        $args_start, $args_end);
            pos($code) = $args_end + 1;
        }
    }

    pos($code) = 0;
    while ($code =~ /(?<![A-Za-z0-9_])str_printf(?![A-Za-z0-9_])/g) {
        my $paren_idx = skip_space_comments($source, $-[0] + 10);
        my $args_start;
        my $args_end;

        if (substr($source, $paren_idx, 1) ne '(') {
            next;
        }

        $args_start = $paren_idx + 1;
        $args_end = call_end($code, $args_start);
        if ($args_end >= 0) {
            report_single_arg_format_string($path, $source, $code,
                                            $args_start, $args_end);
            pos($code) = $args_end + 1;
        }
    }
    # Do not cache diagnostics if the file changed during inspection.
    my $after = file_signature($path);
    if (defined($before) && defined($after) && $before eq $after) {
        if (ref($entry) ne 'HASH'
                || !defined($entry->{signature})
                || !defined($entry->{diagnostics})
                || ref($entry->{signature})
                || ref($entry->{diagnostics})
                || $entry->{signature} ne $after
                || $entry->{diagnostics} ne $file_diagnostics) {
            $cached_files->{$key} = {
                signature => $after,
                diagnostics => $file_diagnostics,
            };
            $cache_dirty = 1;
        }
    } elsif (exists $cached_files->{$key}) {
        delete $cached_files->{$key};
        $cache_dirty = 1;
    }
}

write_cache($cache_path, $checker_hash, $cached_files) if $cache_dirty;
