#!/bin/sh

# usage: $0 <entry>

n="$(echo "$1" | awk 'NR==1{if ($1 + 0 == $1) print $1; exit}')"
[ -z "$n" ] && exit
clipsim --info "$n"
