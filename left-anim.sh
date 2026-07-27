#!/bin/bash
# left-anim.sh — Cycles through cyberpunk animations inside xwinwrap xterm
# Animations: cmatrix → pipes.sh → cbonsai → starfield/glitch art

export TERM=xterm-256color
export PATH="$HOME/bin:/home/sword/miniconda3/bin:$PATH"

CHILD_PID=""

cleanup() {
    [ -n "$CHILD_PID" ] && kill "$CHILD_PID" 2>/dev/null && wait "$CHILD_PID" 2>/dev/null
    pkill -P $$ 2>/dev/null || true
    tput cnorm 2>/dev/null || true
    clear
    exit 0
}
trap cleanup SIGTERM SIGINT EXIT

show_transition() {
    local label="${1:-SYSTEM}"
    clear
    tput civis 2>/dev/null || true
    # Try pyfiglet + lolcat for a glowing header
    if python3 -c "import pyfiglet" 2>/dev/null; then
        python3 -c "
import pyfiglet, sys
try:
    print(pyfiglet.figlet_format('${label}', font='banner3-D'))
except:
    print(pyfiglet.figlet_format('${label}'))
" 2>/dev/null | lolcat -a -d 2 2>/dev/null || \
        python3 -c "import pyfiglet; print(pyfiglet.figlet_format('${label}'))" 2>/dev/null
    else
        echo ">>> ${label} <<<"
    fi
    sleep 1
    tput cnorm 2>/dev/null || true
    clear
}

run_for_duration() {
    local duration=$1; shift
    eval "$*" &
    CHILD_PID=$!
    local elapsed=0
    while [ "$elapsed" -lt "$duration" ] && kill -0 "$CHILD_PID" 2>/dev/null; do
        sleep 1
        elapsed=$(( elapsed + 1 ))
    done
    kill "$CHILD_PID" 2>/dev/null
    wait "$CHILD_PID" 2>/dev/null
    CHILD_PID=""
}

starfield_animation() {
    local duration=${1:-45}
    local cols; cols=$(tput cols 2>/dev/null || echo 80)
    local lines; lines=$(tput lines 2>/dev/null || echo 24)

    local -a chars=('░' '▒' '▓' '█' '▄' '▀' '■' '□' '▪' '▫' '·' '•' '+' '×')
    local -a glitch_chars=('▓' '▒' '░' '█' '╬' '╪' '┼' '═' '║' '╣' '╠' '╦' '╩' '▀' '▄' '▬' '▭')

    # ANSI 256-color codes: cyan/green/blue palette
    local -a colors=(
        '\033[38;5;51m' '\033[38;5;87m' '\033[38;5;123m'
        '\033[38;5;46m' '\033[38;5;82m' '\033[38;5;48m'
        '\033[38;5;21m' '\033[38;5;27m' '\033[38;5;33m'
        '\033[38;5;43m' '\033[38;5;50m' '\033[38;5;35m'
    )
    local -a glitch_colors=(
        '\033[38;5;196m' '\033[38;5;201m' '\033[38;5;199m' '\033[38;5;160m'
    )
    local reset='\033[0m'
    local nc=${#colors[@]}
    local ngc=${#glitch_colors[@]}
    local nch=${#chars[@]}
    local nglch=${#glitch_chars[@]}

    tput civis 2>/dev/null || true
    tput clear 2>/dev/null || clear

    local start=$SECONDS frame=0
    while [ $(( SECONDS - start )) -lt "$duration" ]; do
        # Update ~5% of cells per frame
        local updates=$(( cols * lines / 20 ))
        for (( i=0; i<updates; i++ )); do
            local x=$(( RANDOM % cols ))
            local y=$(( RANDOM % lines ))
            printf "\033[%d;%dH${colors[$(( RANDOM % nc ))]}${chars[$(( RANDOM % nch ))]}${reset}" \
                $(( y + 1 )) $(( x + 1 ))
        done

        # Shooting star streak (every 3 frames)
        if (( frame % 3 == 0 )); then
            local sy=$(( RANDOM % lines ))
            local sx=$(( RANDOM % (cols - 12) ))
            local slen=$(( RANDOM % 8 + 3 ))
            printf "\033[%d;%dH${colors[$(( RANDOM % nc ))]}" $(( sy + 1 )) $(( sx + 1 ))
            for (( s=0; s<slen; s++ )); do printf '▬'; done
            printf "${reset}"
        fi

        # Glitch horizontal line (10% chance)
        if (( RANDOM % 10 == 0 )); then
            local gy=$(( RANDOM % lines ))
            printf "\033[%d;1H${glitch_colors[$(( RANDOM % ngc ))]}" $(( gy + 1 ))
            for (( c=0; c<cols; c++ )); do
                printf "${glitch_chars[$(( RANDOM % nglch ))]}"
            done
            printf "${reset}"
        fi

        # Glitch burst (1% chance)
        if (( RANDOM % 100 == 0 )); then
            local burst=$(( RANDOM % 5 + 2 ))
            for (( b=0; b<burst; b++ )); do
                local by=$(( RANDOM % lines ))
                local bx=$(( RANDOM % (cols / 2) ))
                local blen=$(( RANDOM % 20 + 10 ))
                printf "\033[%d;%dH${glitch_colors[$(( RANDOM % ngc ))]}" $(( by + 1 )) $(( bx + 1 ))
                for (( bc=0; bc<blen; bc++ )); do
                    printf "${glitch_chars[$(( RANDOM % nglch ))]}"
                done
                printf "${reset}"
            done
        fi

        sleep 0.05
        (( frame++ )) || true
    done

    tput cnorm 2>/dev/null || true
}

# ── Main animation loop ────────────────────────────────────────────────
main() {
    clear
    while true; do
        # 1. cmatrix — classic green digital rain
        show_transition "MATRIX"
        run_for_duration 45 "cmatrix -b -u 2 -C cyan"

        # 2. pipes.sh — colorful flowing pipes
        show_transition "PIPES"
        run_for_duration 45 "$HOME/bin/pipes.sh -p 5 -r 0 -R"

        # 3. cbonsai — growing ASCII bonsai tree
        show_transition "BONSAI"
        run_for_duration 45 "$HOME/bin/cbonsai -l -t 0.03"

        # 4. Custom starfield + glitch art
        show_transition "SECTOR7"
        starfield_animation 45
    done
}

main
