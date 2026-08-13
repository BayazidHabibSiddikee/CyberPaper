#!/bin/bash
# cyberdesk.sh — launcher for the single-window PySide6 cyberdeck app
set -o pipefail
export DISPLAY="${DISPLAY:-:0}"
export PATH="$HOME/bin:/home/sword/miniconda3/bin:$PATH"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CFG_DIR="$HOME/.config/animated-wallpaper"
PID_FILE="$CFG_DIR/cyberdeck.pid"
GLAVA_PID_FILE="$CFG_DIR/glava.pid"
LOG_FILE="$CFG_DIR/cyberdeck.log"
GLAVA_MODE_FILE="$CFG_DIR/glava.mode"      # graph, bars, wave, radial
GLAVA_THEME_FILE="$CFG_DIR/glava.theme"    # cyan, green, purple, red, rainbow, sunset
mkdir -p "$CFG_DIR"

# Left column geometry (must match LEFT_PCT / BOTTOM_H in cyberdeck.py)
LEFT_PCT=28
CENTER_PCT=44
BOTTOM_H=32

screen_size() {
    xrandr --current 2>/dev/null | awk '/\*/{print $1; exit}'
}

# ── GLava state helpers ─────────────────────────────────────────────
GLAVA_MODES=("graph" "bars" "wave" "radial")
GLAVA_THEMES=("cyan" "green" "purple" "red" "rainbow" "sunset")

_glava_get_mode() {
    if [ -f "$GLAVA_MODE_FILE" ]; then
        cat "$GLAVA_MODE_FILE"
    else
        echo "graph"
    fi
}

_glava_get_theme() {
    if [ -f "$GLAVA_THEME_FILE" ]; then
        cat "$GLAVA_THEME_FILE"
    else
        echo "cyan"
    fi
}

_glava_is_running() {
    [ -f "$GLAVA_PID_FILE" ] && kill -0 "$(cat "$GLAVA_PID_FILE")" 2>/dev/null
}

# Restart GLava with current settings (preserves mode/theme)
_restart_glava() {
    local mode theme
    mode="$(_glava_get_mode)"
    theme="$(_glava_get_theme)"

    # Kill existing glava + xwinwrap
    if [ -f "$GLAVA_PID_FILE" ]; then
        kill "$(cat "$GLAVA_PID_FILE")" 2>/dev/null
        rm -f "$GLAVA_PID_FILE"
    fi
    pkill -x glava 2>/dev/null
    pkill xwinwrap 2>/dev/null
    sleep 0.2

    if ! command -v glava >/dev/null; then
        return
    fi

    if ! command -v xwinwrap >/dev/null; then
        echo "warning: xwinwrap not installed — glava will NOT be click-through" >&2
        echo "install it (usually 'xwinwrap' or 'xwinwrap-git' in your package manager)" >&2
    fi

    local res sw sh gw gh gy gpid
    res="$(screen_size)"; res="${res:-1920x1080}"
    sw="${res%x*}"; sh="${res#*x}"
    # glava enforces a 50px minimum window height but honours larger heights.
    # Make it a prominent bottom visualizer (~140px) whose bottom edge lands on
    # the top of the 32px dock bar, so it never spills over it.
    gw=$sw; gh=140; gy=$(( sh - BOTTOM_H - gh ))

    # NOTE: we deliberately do NOT pass `--desktop` to glava. With `--desktop`
    # glava forces its own window to 0,0 (the TOP-LEFT of the screen) and
    # ignores the requested geometry, so the visualizer ended up at the top
    # instead of the bottom. Dropping it lets glava honour the explicit
    # `setgeometry 0 <gy> ...` Y below, placing it along the bottom edge,
    # just above the dock bar. xwinwrap still provides click-through (-ni),
    # on-top stacking (remove -b, raise below) and ARGB transparency (-argb).
    if command -v xwinwrap >/dev/null; then
        # -ni  = click-through (empty input shape — the ONLY reliable way to
        #        guarantee the mouse passes straight through to windows below)
        # -ov  = override_redirect, so the WM never restacks it below apps
        # -argb = ARGB visual so the background is truly transparent (compositor
        #        blends the bars over whatever is underneath)
        # NOTE: deliberately no -b (below) — we want this ON TOP of all windows.
        nohup xwinwrap -ov -ni -argb -g ${gw}x${gh}+0+${gy} -- \
            glava -m "$mode" -r "setgeometry 0 $gy $gw $gh" \
            > "$CFG_DIR/glava.log" 2>&1 &
    else
        nohup glava -m "$mode" \
            -r "setgeometry 0 $gy $gw $gh" \
            > "$CFG_DIR/glava.log" 2>&1 &
    fi
    gpid=$!
    echo "$gpid" > "$GLAVA_PID_FILE"

    sleep 0.5
    _apply_glava_theme "$theme"

    # Make glava an ON-TOP overlay (above all normal windows):
    #   - click-through: mouse passes through (empty input shape via -ni)
    #   - transparent background: ARGB visual composited by picom
    #     (rc.glsl setopacity "native" + setbg 00000000)
    # xwinwrap launches glava as a managed window, so we re-class it on each
    # start to keep that overlay behaviour. Search by the PID we just spawned so
    # a stale GLava window from a previous toggle is never grabbed by mistake.
    if command -v xdotool >/dev/null && command -v xprop >/dev/null; then
        gid=$(xdotool search --pid "$gpid" --class GLava 2>/dev/null | head -1)
        [ -z "$gid" ] && gid=$(xdotool search --class GLava 2>/dev/null | head -1)
        if [ -n "$gid" ]; then
            # UTILITY type = no taskbar/pager entry, won't steal keyboard focus.
            xprop -id "$gid" -f _NET_WM_WINDOW_TYPE 32a                 -set _NET_WM_WINDOW_TYPE _NET_WM_WINDOW_TYPE_UTILITY
            # ABOVE = composited above ALL windows ("in front of everything").
            # STICKY = stays visible across workspaces.
            xprop -id "$gid" -f _NET_WM_STATE 32a                 -set _NET_WM_STATE _NET_WM_STATE_ABOVE,_NET_WM_STATE_SKIP_TASKBAR,_NET_WM_STATE_SKIP_PAGER,_NET_WM_STATE_STICKY
            # NOTE: these hints alone do NOT float the window under i3. i3 does
            # not auto-float a UTILITY window, so it TILES glava (we watched it
            # snap to a 945x1024 column, the black block covering the screen).
            # Forcing i3 to float + sticky it makes it render at its natural
            # 1920x140 strip and keeps it pinned ABOVE every other window. On
            # non-i3 WMs the hints above plus xwinwrap's -ov ordering already win.
            if command -v i3-msg >/dev/null; then
                i3-msg "[id=$gid] floating enable, sticky enable" >/dev/null 2>&1 || true
            fi
            xdotool windowraise "$gid" 2>/dev/null || true
        fi
    fi
}


# Apply color theme to GLava graph shader
_apply_glava_theme() {
    local theme="$1"
    local glsl="$HOME/.config/glava/graph.glsl"
    local bars_glsl="$HOME/.config/glava/bars.glsl"
    local radial_glsl="$HOME/.config/glava/radial.glsl"

    case "$theme" in
        cyan)
            _sed_color "$glsl" "#00D4FF" "#00FFC8"
            _sed_color "$bars_glsl" "#00D4FF" "#00FFC8"
            _sed_color "$radial_glsl" "#00D4FF" "#00FFC8"
            ;;
        green)
            _sed_color "$glsl" "#00FF88" "#88FF00"
            _sed_color "$bars_glsl" "#00FF88" "#88FF00"
            _sed_color "$radial_glsl" "#00FF88" "#88FF00"
            ;;
        purple)
            _sed_color "$glsl" "#c678dd" "#7c3aed"
            _sed_color "$bars_glsl" "#c678dd" "#7c3aed"
            _sed_color "$radial_glsl" "#c678dd" "#7c3aed"
            ;;
        red)
            _sed_color "$glsl" "#e06c75" "#ff4444"
            _sed_color "$bars_glsl" "#e06c75" "#ff4444"
            _sed_color "$radial_glsl" "#e06c75" "#ff4444"
            ;;
        rainbow)
            _sed_color "$glsl" "#ff0000" "#0000ff"
            _sed_color "$bars_glsl" "#ff0000" "#0000ff"
            _sed_color "$radial_glsl" "#ff0000" "#0000ff"
            ;;
        sunset)
            _sed_color "$glsl" "#ff6b35" "#ff006e"
            _sed_color "$bars_glsl" "#ff6b35" "#ff006e"
            _sed_color "$radial_glsl" "#ff6b35" "#ff006e"
            ;;
    esac
}

_sed_color() {
    local file="$1" c1="$2" c2="$3"
    [ -f "$file" ] || return
    # Replace the mix() color pair in the shader
    sed -i "s|mix(#[0-9A-Fa-f]\{6\}, #[0-9A-Fa-f]\{6\}|mix(${c1}, ${c2}|g" "$file"
}

start() {
    # Safety net: kill any stray cyberdeck instance first. Two decks = two
    # 'sworddeck-bar' dock windows, which i3 stacks on top of each other at the
    # bottom of the screen (one at y=1016, one at y=1048) — the "double bar"
    # seen when start() was called while an older, untracked deck was alive.
    pkill -f "cyberdeck\.py" 2>/dev/null
    sleep 0.3

    if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo "cyberdeck already running (PID $(cat "$PID_FILE")). Use restart."
        exit 0
    fi

    # Render initial graph PNG
    [ -f "$CFG_DIR/graph.json" ] || cp "$HERE/graph.default.json" "$CFG_DIR/graph.json"
    "$HERE/graph-render.sh" > "$CFG_DIR/graph-render.log" 2>&1 || true

    # Init default state files
    [ -f "$GLAVA_MODE_FILE" ] || echo "graph" > "$GLAVA_MODE_FILE"
    [ -f "$GLAVA_THEME_FILE" ] || echo "cyan" > "$GLAVA_THEME_FILE"

    # Launch GLava with saved settings
    GLAVA=0
    if command -v glava >/dev/null; then
        _restart_glava
        _glava_is_running && GLAVA=1
    fi

    # Launch single Python window
    # Built-in SpectrumOverlay stays OFF when real glava is drawing
    CYBERDECK_GLAVA=0 nohup python3 "$HERE/cyberdeck.py" > "$LOG_FILE" 2>&1 &
    echo $! > "$PID_FILE"
    echo "cyberdeck started (PID $!, glava=$GLAVA)"
}

stop() {
    if [ -f "$PID_FILE" ]; then
        pid=$(cat "$PID_FILE")
        kill "$pid" 2>/dev/null && echo "cyberdeck stopped (PID $pid)" || echo "already dead"
        rm -f "$PID_FILE"
    fi
    # Always sweep strays so a stop/restart can never leave a duplicate bar.
    pkill -f "cyberdeck\.py" 2>/dev/null
    if [ -f "$GLAVA_PID_FILE" ]; then
        kill "$(cat "$GLAVA_PID_FILE")" 2>/dev/null
        rm -f "$GLAVA_PID_FILE"
    fi
    pkill -x glava 2>/dev/null
    pkill xwinwrap 2>/dev/null
}

toggle_glava() {
    if _glava_is_running; then
        kill "$(cat "$GLAVA_PID_FILE")" 2>/dev/null
        rm -f "$GLAVA_PID_FILE"
        pkill -x glava 2>/dev/null
        echo "glava stopped"
    else
        if command -v glava >/dev/null; then
            _restart_glava
            echo "glava started (PID $(cat "$GLAVA_PID_FILE"))"
        else
            echo "glava not installed"
        fi
    fi
}

glava_mode() {
    local new_mode="${1:-}"
    if [ -z "$new_mode" ]; then
        echo "current mode: $(_glava_get_mode)"
        echo "available: ${GLAVA_MODES[*]}"
        return
    fi
    # Validate
    local valid=0
    for m in "${GLAVA_MODES[@]}"; do
        [ "$m" = "$new_mode" ] && valid=1
    done
    if [ "$valid" -eq 0 ]; then
        echo "invalid mode: $new_mode (available: ${GLAVA_MODES[*]})"
        return 1
    fi
    echo "$new_mode" > "$GLAVA_MODE_FILE"
    if _glava_is_running; then
        _restart_glava
        echo "glava mode: $new_mode (restarted)"
    else
        echo "glava mode set to $new_mode (will apply on start)"
    fi
}

glava_theme() {
    local new_theme="${1:-}"
    if [ -z "$new_theme" ]; then
        echo "current theme: $(_glava_get_theme)"
        echo "available: ${GLAVA_THEMES[*]}"
        return
    fi
    # Validate
    local valid=0
    for t in "${GLAVA_THEMES[@]}"; do
        [ "$t" = "$new_theme" ] && valid=1
    done
    if [ "$valid" -eq 0 ]; then
        echo "invalid theme: $new_theme (available: ${GLAVA_THEMES[*]})"
        return 1
    fi
    echo "$new_theme" > "$GLAVA_THEME_FILE"
    if _glava_is_running; then
        _restart_glava
        echo "glava theme: $new_theme (restarted)"
    else
        echo "glava theme set to $new_theme (will apply on start)"
    fi
}

glava_status() {
    local mode theme running
    mode="$(_glava_get_mode)"
    theme="$(_glava_get_theme)"
    running=$(_glava_is_running && echo "yes" || echo "no")
    echo "glava running=$running mode=$mode theme=$theme"
}

status() {
    if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo "running (PID $(cat "$PID_FILE"))"
    else
        echo "not running"
    fi
}

case "${1:-start}" in
    start)        start ;;
    stop)         stop ;;
    restart)      stop; sleep 1; start ;;
    status)       status ;;
    edit)         "$HERE/graph-edit.sh" ;;
    glava-toggle) toggle_glava ;;
    glava-mode)   glava_mode "${2:-}" ;;
    glava-theme)  glava_theme "${2:-}" ;;
    glava-status) glava_status ;;
    *) echo "Usage: cyberdesk.sh [start|stop|restart|status|edit|glava-toggle|glava-mode [graph|bars|wave|radial]|glava-theme [cyan|green|purple|red|rainbow|sunset]|glava-status]"; exit 1 ;;
esac
