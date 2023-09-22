# Save this as _clipsim.bash in a directory listed in your $BASH_COMPLETION_DIR, such as /etc/bash_completion.d/ or your own custom directory.

_clipsim_entries() {
  local entries
  entries=$(clipsim -p | grep -oP '^\s*\K([0-9]+)(?=\s*:)' | tr '\n' ' ')
  COMPREPLY=($(compgen -W "$entries" -- "${COMP_WORDS[COMP_CWORD]}"))
}

_clipsim_completion() {
  local cur prev commands
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  commands=(
    "-p --print"
    "-i --info"
    "-c --copy"
    "-r --remove"
    "-s --save"
    "-d --daemon"
    "-h --help"
  )

  case "${prev}" in
    -i|--info|-c|--copy|-r|--remove)
      _clipsim_entries
      return
      ;;
  esac

  COMPREPLY=($(compgen -W "${commands[*]}" -- "${cur}"))
}

complete -F _clipsim_completion clipsim
