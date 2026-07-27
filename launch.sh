#!/bin/bash
# Animated Wallpaper Launcher — for i3
DIR="$(cd "$(dirname "$0")" && pwd)"
PID_FILE="$HOME/.config/animated-wallpaper/pid"
mkdir -p "$(dirname "$PID_FILE")"

case "$1" in
    --stop|-s)
        [ -f "$PID_FILE" ] && kill "$(cat "$PID_FILE")" 2>/dev/null
        rm -f "$PID_FILE"
        echo "Stopped."
        exit 0
        ;;
    --status)
        if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
            echo "Running (PID $(cat "$PID_FILE"))"
        else
            echo "Not running."
        fi
        exit 0
        ;;
    --next|-n)
        [ -f "$PID_FILE" ] && kill "$(cat "$PID_FILE")" 2>/dev/null
        rm -f "$PID_FILE"
        setsid python3 "$DIR/wallpaper.py" --next > /dev/null 2>&1 &
        sleep 1
        NEWPID=$(ps aux | grep 'wallpaper.py' | grep -v grep | awk '{print $2}' | head -1)
        [ -n "$NEWPID" ] && echo $NEWPID > "$PID_FILE"
        echo "Switched."
        exit 0
        ;;
    --help|-h)
        echo "Usage: launch.sh [gradient|particles|wave|matrix|graph|--next|--stop|--status]"
        exit 0
        ;;
esac

# Stop old
[ -f "$PID_FILE" ] && kill "$(cat "$PID_FILE")" 2>/dev/null
rm -f "$PID_FILE"
sleep 0.5

MODE="${1:-gradient}"
setsid python3 "$DIR/wallpaper.py" --mode "$MODE" > /dev/null 2>&1 &
sleep 2

NEWPID=$(ps aux | grep 'wallpaper.py' | grep -v grep | awk '{print $2}' | head -1)
if [ -n "$NEWPID" ]; then
    echo $NEWPID > "$PID_FILE"
    echo "Wallpaper running — mode: $MODE (PID $NEWPID)"
else
    echo "Wallpaper failed to start."
    exit 1
fi
