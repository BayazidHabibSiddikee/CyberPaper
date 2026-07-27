# Cyberdesk — bash/C-tools version (no Python)

Same idea as before — matrix rain + audio bars on the left, mission graph on
the right, clock/CPU/RAM/disk/net bar on the bottom — built entirely from
small existing Linux programs instead of a custom Qt app. Copy this whole
folder anywhere (e.g. `~/animated-wallpaper-bash/`) on any Linux box and it
runs the same way, no interpreter or build step required.

## Install (Arch)
Everything except `xwinwrap` is in the official repos:
```bash
sudo pacman -S cmatrix cava conky jq graphviz rofi xterm playerctl libnotify xorg-xrandr
```
`xwinwrap` is AUR-only:
```bash
yay -S xwinwrap-git
# or with paru: paru -S xwinwrap-git
```
No `yay`/`paru` yet:
```bash
git clone https://aur.archlinux.org/xwinwrap-git.git
cd xwinwrap-git && makepkg -si
```

<details>
<summary>Debian/Ubuntu/Fedora (if you ever run this on a non-Arch box)</summary>

```bash
sudo apt install cmatrix cava conky-all jq graphviz rofi xterm playerctl \
                  libnotify-bin x11-xserver-utils
```
`xwinwrap` needs a manual build (`libx11-dev libxrender-dev` only):
```bash
git clone https://github.com/mmhobi7/xwinwrap && cd xwinwrap && make && sudo make install
```
</details>

## Run it
```bash
chmod +x *.sh
./cyberdesk.sh start      # or: restart / stop / status / edit
```
Add `i3-cyberdesk-bash.conf`'s contents to your i3 config for autostart +
keybinds (`$mod+Ctrl+6` restart, `$mod+Ctrl+Escape` stop, `$mod+Ctrl+e`
open the graph editor).

## Editing your mission graph
```bash
./cyberdesk.sh edit
```
Opens a `rofi` menu: Add Task, Link Tasks (draws an arrow), Set Status,
Rename, Delete. It edits `~/.config/animated-wallpaper/graph.json` directly
(plain JSON, no schema surprises) and re-renders the PNG immediately. The
background watcher also re-renders automatically if you edit that JSON file
by hand or with any other tool.

## What I actually verified vs. what needs your first real test
I don't have a display in my sandbox, so I could fully test and confirm:
- the `jq` graph transforms (add/link/status/delete) — verified against real data
- the `graphviz`/`neato` rendering (`graph-render.sh`) — produces a correct PNG
- `net-speed.sh` and all bash syntax

What I could **not** test end-to-end without an X server, and you should
sanity-check on first run:
- **xwinwrap flags** — I used the common `mmhobi7/xwinwrap` fork's flags
  (`-ov -b -ni -nf -g`). If your installed fork differs, run
  `xwinwrap --help` and compare; the flags are marked `[XWINWRAP FLAGS]` in
  `cyberdesk.sh` so they're easy to find and adjust.
- **`xterm -into %WIN`** — this is how cmatrix/cava get embedded into the
  wallpaper-positioned window. It's a real, documented xterm feature, but
  if your xterm build lacks it, swap in `urxvt -embed %WIN` instead (urxvt
  supports the same trick).
- **conky's `own_window_hints`** — `below,sticky,skip_taskbar,skip_pager`
  should keep both conky panels beneath your windows and off the taskbar;
  some WMs/compositors need `own_window_type = 'override'` instead of
  `'desktop'` to actually stay below. If a panel steals focus or sits on
  top, that's the line to flip first.

If any panel doesn't show up, run `./cyberdesk.sh status` and check the
`log-*.txt` files in `~/.config/animated-wallpaper/` — each panel logs its
own stdout/stderr there.

## Tuning
- Left column width, bottom bar height, matrix-vs-cava split: the three
  variables at the top of `cyberdesk.sh` (`LEFT_FRACTION`, `BOTTOM_H`,
  `MATRIX_FRACTION`).
- Matrix color/speed: edit the `xterm ... -e cmatrix -b -u 4` line
  (`-u` = speed, `-b` = bold; see `man cmatrix`).
- Audio bar look: `cava-config` (colors, bar count, smoothing).
- Graph colors/layout engine: `graph-render.sh` (swap `neato` for `fdp` or
  `dot` for a different layout style; `man graphviz`).
