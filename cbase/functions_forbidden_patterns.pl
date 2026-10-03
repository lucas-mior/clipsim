use strict;
use warnings;

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

        if ($ch =~ /\s/) {
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
                print "$path:$line:STREQUAL called with STRLIT literal\n";
                return;
            }

            $arg_start = $idx + 1;
        }

        $idx += 1;
    }

    return;
}

for my $path (@paths) {
    open my $fh, '<', $path or die "$path: $!\n";
    local $/;
    my $source = <$fh>;
    my $code = code_mask($source);

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
            print "$path:$line:STRLIT_LEN called on a literal string\n";
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
}
