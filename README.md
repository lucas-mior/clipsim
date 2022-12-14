# Clipsim

clipsim is a simple X11 clipboard manager written in C.
It retrives clipboard text when the window owning it is closed,
and keeps a clipboard history.
Images are ignored.
The primary and secondary selection buffers are also ignored.
When copying text equal to some previous text, the history order
is updated so that each entry is unique in the history.

## Usage

First, you need to start the daemon by running
```
clipsim daemon
```

Start copying text into the X11 clipboard.

Then, when you want to print the history, run:
```
clipsim print
```

Which will print the entire clipboard history,
with entries separated by `NULL`.
And with multiple white space supressed.

In order to select one of them, you can use
[fzf](https://github.com/junegunn/fzf)
and [xsel](https://github.com/kfish/xsel)
```
clipsim print | fzf --read0 | xsel -b
```

In order to retrieve the entry with original white space and
remove the leading number:

```
#!/bin/sh

n=$(clipsim print | fzf --read0 | head -n 1 | cut -d ' ' -f 1)
[ -n "$n" ] && clipsim copy "$n"
```

Tip: use with a terminal emulator that opens quickly.
My suggestion is [urxvtc](https://linux.die.net/man/1/urxvtc)
(be sure to have [urxvtd](https://linux.die.net/man/1/urxvtd) running):

```
urxvtc -e clip.sh # clip.sh is the script above
```
If you know of some terminal emulator that opens faster than urxvtc,
please let me know.

To explicity save the clipboard history in `$XDG_CACHE_HOME/clipsim/history`:
```
clipsim save
```

In order to delete an specific entry from history:
```
clipsim delete <N>
```

In order to print an specific entry from history:
```
clipsim info <N>
```

## Instalation
Make sure you have [libxfixes](https://gitlab.freedesktop.org/xorg/lib/libxfixes)
and [xsel](https://github.com/kfish/xsel) installed.
```
$ git clone https://codeberg.org/lucas.mior/clipsim.git clipsim
$ cd clipsim
$ make
# make install
```

## Configuration
Edit `config.h` and recompile.

## Bugs
Clipsim *might* have an weird behavior if you use it with applications that do
not use UTF-8.
Additionally, incremental buffer reading is not implemented,
which means that if an application is to use that so called "feature",
clipsim won't save that text to history.

## Rationale
There are many other clipboard managers for X11,
however all of them are either too slow,
not modular (enforce an usage method upon users)
or don't recover clipboard when a window is closed.

Clipsim adresses this issues.
