# Save this as _clipsim.fish in a directory listed in your $fish_complete_path, such as ~/.config/fish/completions/

complete -c clipsim -l print -d 'print entire history, with trimmed whitespace'
complete -c clipsim -s p -d 'print entire history, with trimmed whitespace'
complete -c clipsim -l info -d 'print entry number <n>, with original whitespace' -a '(_clipsim_entries)'
complete -c clipsim -s i -d 'print entry number <n>, with original whitespace' -a '(_clipsim_entries)'
complete -c clipsim -l copy -d 'copy entry number <n>, with original whitespace' -a '(_clipsim_entries)'
complete -c clipsim -s c -d 'copy entry number <n>, with original whitespace' -a '(_clipsim_entries)'
complete -c clipsim -l remove -d 'remove entry number <n>' -a '(_clipsim_entries)'
complete -c clipsim -s r -d 'remove entry number <n>' -a '(_clipsim_entries)'
complete -c clipsim -l save -d 'save history to $XDG_CACHE_HOME/clipsim/history'
complete -c clipsim -s s -d 'save history to $XDG_CACHE_HOME/clipsim/history'
complete -c clipsim -l daemon -d 'spawn daemon (clipboard watcher and command listener)'
complete -c clipsim -s d -d 'spawn daemon (clipboard watcher and command listener)'
complete -c clipsim -l help -d 'print this help message'
complete -c clipsim -s h -d 'print this help message'

function _clipsim_entries
  set -l entries (clipsim -p | pcregrep -o1 '^\\s*([0-9]+)\\s*:.*$')
  complete -c clipsim -a "($entries)"
end

_clipsim_entries
