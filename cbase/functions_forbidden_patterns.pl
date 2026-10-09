use strict;
use warnings;
use Cwd qw(abs_path);
use Digest::SHA;
use File::Basename qw(dirname);
use File::Path qw(make_path);
use File::Temp qw(tempfile);
use JSON::PP qw(decode_json encode_json);
use Time::HiRes qw(stat);

my $file_diagnostics = '';
my @paths = @ARGV;

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

my $cache_path = $ENV{CBASE_FORBIDDEN_PATTERNS_CACHE}
                 // '.cache/functions_forbidden_patterns.json';
my $checker_hash = checker_fingerprint();
my $cached_files = read_cache($cache_path, $checker_hash);
my $cache_dirty = 0;

sub line_number {
    my ($text, $idx) = @_;
    return (substr($text, 0, $idx) =~ tr/\n//) + 1;
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
        $fits = length($flat_text) <= $column_limit;

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

for my $path (@paths) {
    my $absolute_path = abs_path($path) // $path;
    my $column_limit = $absolute_path =~ m{(?:\A|/)cecup/src/}
                       ? 100 : 80;

    # The displayed path is part of the diagnostic, so distinguish spellings
    # such as "file.c" and "./file.c" even if both resolve to the same file.
    my $key = join("\0", $path, $absolute_path);
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
    my $code = code_mask($source);

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
                || substr($source, $paren_idx + 1) !~ /^[ \t]*\r?\n/) {
            next;
        }

        my $line = line_number($source, $paren_idx);
        report_issue("$path:$line:$name call breaks before the first "
                     . "argument\n");
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
            my @arg_starts;
            my @arg_ends;
            my @arg_has_newline;
            my $idx;
            my $paren_depth = 0;
            my $bracket_depth = 0;
            my $brace_depth = 0;
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

            # printf-like calls align wrapped arguments with the format
            # string. Detect that argument by looking for a '%' inside a
            # string literal, so local wrappers follow the same rule.
            for (my $arg_idx = 0;
                 $arg_idx < scalar(@arg_starts); $arg_idx += 1) {
                if (range_has_percent_string_literal($source,
                                                     $arg_starts[$arg_idx],
                                                     $arg_ends[$arg_idx])) {
                    $format_arg_idx = $arg_idx;
                    last;
                }
            }

            if ($format_arg_idx >= 0
                    && $format_arg_idx < scalar(@arg_starts)) {
                $alignment_arg = $format_arg_idx;
            }

            if ($alignment_arg >= scalar(@arg_starts)) {
                $alignment_arg = 0;
            }
            $anchor_idx = $arg_starts[$alignment_arg];
            $anchor_line_start = 0;
            if ($anchor_idx > 0) {
                $anchor_line_start = rindex($source, "\n", $anchor_idx - 1)
                                     + 1;
            }
            $anchor_column = $anchor_idx - $anchor_line_start;

            for (my $arg_idx = $alignment_arg + 1;
                 $arg_idx < scalar(@arg_starts); $arg_idx += 1) {
                my $current_idx;
                my $current_line_start = 0;
                my $current_column;
                my $line;

                next unless $arg_has_newline[$arg_idx];
                $current_idx = $arg_starts[$arg_idx];
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
                    && $format_arg_idx < scalar(@arg_starts)
                    && $format_arg_idx + 1 < scalar(@arg_starts)) {
                my $format_line = line_number(
                    $source, $arg_ends[$format_arg_idx]);
                my $format_line_has_args = 0;
                my $other_line_has_args = 0;

                for (my $arg_idx = $format_arg_idx + 1;
                     $arg_idx < scalar(@arg_starts); $arg_idx += 1) {
                    my $line = line_number($source,
                                           $arg_starts[$arg_idx]);

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

            if (scalar(@arg_starts) >= 3) {
                my @arg_names;
                my @arg_lines;
                my %args_per_line;

                for (my $arg_idx = 0;
                     $arg_idx < scalar(@arg_starts); $arg_idx += 1) {
                    my $arg = substr($code, $arg_starts[$arg_idx],
                                     $arg_ends[$arg_idx]
                                     - $arg_starts[$arg_idx]);
                    my $line = line_number($source, $arg_starts[$arg_idx]);

                    if ($arg =~ /^\s*([A-Za-z_][A-Za-z0-9_]*)\s*$/s) {
                        $arg_names[$arg_idx] = $1;
                    }
                    $arg_lines[$arg_idx] = $line;
                    $args_per_line{$line} += 1;
                }

                for (my $arg_idx = 0;
                     $arg_idx + 1 < scalar(@arg_starts); $arg_idx += 1) {
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

    report_unnecessary_call_wraps($path, $source, $code, $column_limit);

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
