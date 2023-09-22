#compdef clipsim

local -a commands
commands=(
    '-p[print entire history, with trimmed whitespace]'
    '--print[print entire history, with trimmed whitespace]'
    '-i[print entry number <n>, with original whitespace]: :_clipsim_entries'
    '--info[print entry number <n>, with original whitespace]: :_clipsim_entries'
    '-c[copy entry number <n>, with original whitespace]: :_clipsim_entries'
    '--copy[copy entry number <n>, with original whitespace]: :_clipsim_entries'
    '-r[remove entry number <n>]: :_clipsim_entries'
    '--remove[remove entry number <n>]: :_clipsim_entries'
    '-s[save history to $XDG_CACHE_HOME/clipsim/history]'
    '--save[save history to $XDG_CACHE_HOME/clipsim/history]'
    '-d[spawn daemon (clipboard watcher and command listener)]'
    '--daemon[spawn daemon (clipboard watcher and command listener)]'
    '-h[print help information]'
    '--help[print help information]'
)

_clipsim_entries() {
    _arguments ':entry number:(( $(clipsim -p | sed -nE "s/^\s*([0-9]+)\s*:.*$/\1/p") ))'
}

_arguments -s -S $commands
