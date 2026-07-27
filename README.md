# Sworddeck — animated desktop HUD + status dock for i3

A PySide6 desktop overlay that acts as your wallpaper, plus an always-visible
bottom dock bar that **replaces i3bar**. One project, two windows:

```
┌───────────────┬──────────────────────────────┬───────────────────┐
│  LEFT         │  CENTER                      │  RIGHT            │
│  glava audio  │  clock + date                │  CPU/RAM/DISK     │
│  spectrum     │  mission graph (PNG)         │  network, temps   │
│  (top 30%)    │  [✚ EDIT GRAPH] button       │  top 10 processes │
│  matrix rain  │  nodes/uptime/kernel strip   │  quick keys       │
│  below        │                              │  APPS + SETTINGS  │
├───────────────┴──────────────────────────────┴───────────────────┤
│ DOCK BAR (always visible, replaces i3bar):                       │
│ workspaces · user/time · CPU · RAM · disk · net · music          │
│                          wifi SSID+IP · volume · battery · uptime│
└───────────────────────────────────────────────────────────────────┘
```

- The **wallpaper window** is override-redirect (`Qt.BypassWindowManagerHint`)
  so i3 never tiles it, and re-lowers itself every second so all your apps
  stay on top. Buttons work wherever the desktop is exposed.
- The **dock bar** is a real X11 dock window: i3 reserves its space and it is
  never covered. Workspace numbers are clickable.
- `glava --desktop -m graph` runs as a third transparent window over the top
  of the left column.

## Install on a fresh Arch box

```bash
# 1. Packages
sudo pacman -S python-pyside6 glava jq graphviz rofi playerctl libnotify \
               xorg-xrandr pavucontrol redshift networkmanager \
               ttf-jetbrains-mono maim

# 2. A compositor must be running for transparency (add to i3 config):
sudo pacman -S picom       # exec_always --no-startup-id picom

# 3. Copy this folder
git clone <your-repo> ~/animated-wallpaper   # or rsync/scp the folder
cd ~/animated-wallpaper && chmod +x *.sh

# 4. glava config (module + theme colors)
glava -C                     # copies defaults to ~/.config/glava/
sed -i 's/#request mod bars/#request mod graph/' ~/.config/glava/rc.glsl
sed -i 's/setprintframes true/setprintframes false/' ~/.config/glava/rc.glsl
sed -i 's|mix(#802A2A, #4F4F92|mix(#00D4FF, #00FFC8|' ~/.config/glava/graph.glsl

# 5. First run
./cyberdesk.sh start
```

Debian/Ubuntu: `sudo apt install python3-pyside6.qtwidgets glava jq graphviz
rofi playerctl libnotify-bin pavucontrol redshift network-manager
fonts-jetbrains-mono maim picom` (package names vary slightly).

## i3 integration

Add to `~/.config/i3/config`:

```
# autostart + keybinds
exec_always --no-startup-id ~/animated-wallpaper/cyberdesk.sh restart
bindsym $mod+Ctrl+6      exec ~/animated-wallpaper/cyberdesk.sh restart
bindsym $mod+Ctrl+e      exec ~/animated-wallpaper/graph-edit.sh
bindsym $mod+Ctrl+Escape exec ~/animated-wallpaper/cyberdesk.sh stop
```

**Disable i3bar** (the dock bar replaces it) — comment out the whole
`bar { … }` block in the i3 config, then `i3-msg reload`. Keep a backup;
note you lose the system tray (nm-applet etc.) — add `stalonetray` if you
need tray icons.

## Launcher commands

```bash
./cyberdesk.sh start | stop | restart | status | edit
```

`stop` also pkills stray `glava`/`cyberdeck.py` processes.

## Files

| File              | Purpose                                              |
|-------------------|------------------------------------------------------|
| `cyberdesk.sh`    | Launcher; computes glava geometry from xrandr        |
| `cyberdeck.py`    | Wallpaper window + spawns the dock bar               |
| `left_panel.py`   | Matrix rain / pipes animations (cycle every 45 s)    |
| `center_panel.py` | Clock, mission-graph PNG, EDIT GRAPH button          |
| `right_panel.py`  | Stats, top processes, APPS + SETTINGS buttons        |
| `bottom_bar.py`   | Dock bar: workspaces, stats, wifi, volume, battery   |
| `graph-edit.sh`   | rofi mission-graph editor                            |
| `graph-render.sh` | graph.json → graph.png (graphviz, transparent bg)    |

Runtime state in `~/.config/animated-wallpaper/`: `graph.json`, `graph.png`,
`apps.json`, `redshift.on` flag, logs, PID files.

## Buttons

**Center**: `✚ EDIT GRAPH` → rofi editor (Add Task, Link, Status, Rename,
Delete). Same as `$mod+Ctrl+e` / `./cyberdesk.sh edit`. Hand-edits to
`graph.json` re-render automatically within ~10 s.

**Right panel APPS**: launch entries from
`~/.config/animated-wallpaper/apps.json`
(`[{"name": "Terminal", "cmd": "alacritty"}, …]`). `✚ Add app…` prompts via
rofi and appends; the list auto-rebuilds within 3 s of any file change.

**Right panel SETTINGS**:
- `✎ Edit graph` — rofi editor
- `♪ Audio mixer` — pavucontrol
- `⇄ Wifi on/off` — `nmcli radio wifi` toggle
- `🔇 Mute on/off` — `pactl set-sink-mute @DEFAULT_SINK@ toggle`
- `☾ Reading mode (5000K)` / `☀ Normal colors` — toggles
  `redshift -O 5000` ↔ `redshift -x` (state kept in `redshift.on`)
- `⚙ Config folder`, `⟳ Restart deck`

## Dock bar

- Workspace buttons (left) — click to switch; focused highlighted, urgent red
- `[USERNAME]` + time/date, CPU, RAM, disk %, net ↓/↑ KB/s (computed from
  `/proc/net/dev` between ticks), current track (playerctl)
- Right side: wifi SSID + IP, volume (mute-aware), battery % (amber ≤40%,
  red ≤20%), uptime

## Tuning

- Column widths / bar height: `LEFT_PCT`, `RIGHT_PCT`, `BOTTOM_H` in
  `cyberdeck.py` — keep `LEFT_PCT`/`BOTTOM_H` in sync with `cyberdesk.sh`.
- glava height: `gh=$(( th * 30 / 100 ))` in `cyberdesk.sh` (top 30% of the
  left column). Module: swap `-m graph` for `bars`, `wave`, `radial`.
- Colors: QColor constants at the top of each panel file; spectrum colors in
  `~/.config/glava/graph.glsl`.
- Redshift warmth: change `5000` in `right_panel.py`.

## Troubleshooting

- **Deck invisible** → is a compositor running? Check
  `~/.config/animated-wallpaper/cyberdeck.log`.
- **Flat spectrum** → glava only draws while audio plays; see `glava.log`.
- **Buttons dead** → they need exposed desktop; use keybinds when covered.
- **glava has a glow/shadow** → picom shadow; add
  `shadow-exclude = ["name = 'GLava'"]` to picom.conf.
- **Wrong screen size** → `cyberdeck.py --screen 2560x1440` (auto-detects by
  default; `cyberdesk.sh` uses xrandr for glava).
