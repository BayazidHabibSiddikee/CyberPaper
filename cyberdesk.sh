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

    # glava audio visualizer, stacked below the deck (env_i3 → "!-"),
    # visible through the deck's transparent left column
    GLAVA=0
    if command -v glava >/dev/null; then
        res="$(screen_size)"; res="${res:-1920x1080}"
        sw="${res%x*}"; sh="${res#*x}"
        lw=$(( sw * LEFT_PCT / 100 ))
        th=$(( sh - BOTTOM_H ))
        gh=$(( th * 30 / 100 ))   # glava takes the top 30% of the column
        nohup glava --desktop -m graph \
            -r "setgeometry 0 0 $lw $gh" \
            > "$CFG_DIR/glava.log" 2>&1 &
        echo $! > "$GLAVA_PID_FILE"
        GLAVA=1
    fi

    # Launch single Python window
    CYBERDECK_GLAVA=$GLAVA nohup python3 "$HERE/cyberdeck.py" > "$LOG_FILE" 2>&1 &
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
