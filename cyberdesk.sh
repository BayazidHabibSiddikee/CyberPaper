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
mkdir -p "$CFG_DIR"

# Left column geometry (must match LEFT_PCT / BOTTOM_H in cyberdeck.py)
LEFT_PCT=28
CENTER_PCT=44
BOTTOM_H=32

screen_size() {
    xrandr --current 2>/dev/null | awk '/\*/{print $1; exit}'
}

start() {
    if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo "cyberdeck already running (PID $(cat "$PID_FILE")). Use restart."
        exit 0
    fi

    # Render initial graph PNG
    [ -f "$CFG_DIR/graph.json" ] || cp "$HERE/graph.default.json" "$CFG_DIR/graph.json"
    "$HERE/graph-render.sh" > "$CFG_DIR/graph-render.log" 2>&1 || true

    # Real glava audio visualizer — a borderless, unmanaged window ("!-"
    # from env_i3.glsl) spanning the bottom of the merged left+center
    # panel. The deck re-lowers itself every second, so glava ends up
    # sandwiched: above the deck background, below all normal windows.
    GLAVA=0
    if command -v glava >/dev/null; then
        res="$(screen_size)"; res="${res:-1920x1080}"
        sw="${res%x*}"; sh="${res#*x}"
        gw=$sw                                          # full screen width
        gh=$(( sh * 22 / 100 ))                        # bottom 22% strip
        gy=$(( sh - BOTTOM_H - gh ))
        nohup glava --desktop -m graph \
            -r setgeometry\ 0\ $gy\ $gw\ $gh \
            > "$CFG_DIR/glava.log" 2>&1 &
        GLAVA_PID=$!
        echo $GLAVA_PID > "$GLAVA_PID_FILE"

        # Wait for glava window to appear, then set X11 hints to keep it
        # below normal windows (below apps, above the deck background)
        for i in $(seq 1 20); do
            sleep 0.1
            WID=$(xdotool search --class "GLava" 2>/dev/null | head -1)
            [ -n "$WID" ] && break
        done
        if [ -n "$WID" ]; then
            # DESKTOP type — absolute lowest layer, behind everything
            xprop -id "$WID" \
                -f _NET_WM_WINDOW_TYPE 32a \
                -set _NET_WM_WINDOW_TYPE "_NET_WM_WINDOW_TYPE_DESKTOP" \
                2>/dev/null
            xprop -id "$WID" \
                -f _NET_WM_STATE 32a \
                -set _NET_WM_STATE "_NET_WM_STATE_BELOW,_NET_WM_STATE_SKIP_TASKBAR,_NET_WM_STATE_SKIP_PAGER" \
                2>/dev/null
        fi
        GLAVA=1
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
    else
        pkill -f "python3.*cyberdeck.py" 2>/dev/null && echo "stopped" || echo "not running"
    fi
    if [ -f "$GLAVA_PID_FILE" ]; then
        kill "$(cat "$GLAVA_PID_FILE")" 2>/dev/null
        rm -f "$GLAVA_PID_FILE"
    fi
    pkill -x glava 2>/dev/null
}

status() {
    if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo "running (PID $(cat "$PID_FILE"))"
    else
        echo "not running"
    fi
}

case "${1:-start}" in
    start)   start ;;
    stop)    stop ;;
    restart) stop; sleep 1; start ;;
    status)  status ;;
    edit)    "$HERE/graph-edit.sh" ;;
    *) echo "Usage: cyberdesk.sh [start|stop|restart|status|edit]"; exit 1 ;;
esac
