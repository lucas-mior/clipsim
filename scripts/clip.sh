#!/bin/sh

# usage: $0 [-c |-r]
# usage: $0 [--copy|--remove]

id="$(clipsim --print 2> /dev/null \
    | fzf --prompt="clipsim $1 " \
          --reverse --no-multi-line --read0 --preview='clipinfo.sh {}' \
    | awk 'NR==1{print $1; exit}')"
[ -n "$id" ] && clipsim "$1" "$id"
