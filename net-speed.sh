#!/bin/bash
# net-speed.sh — prints "down_KBs up_KBs" by diffing /proc/net/dev over 1s.
# Sums all interfaces except lo, so it works regardless of interface name
# (eth0, wlan0, enp3s0, etc.) — called by conky via ${execi ...}.

read_bytes() {
    awk 'NR>2 && $1 !~ /lo:/ {
        gsub(":", "", $1)
        rx+=$2; tx+=$10
    } END { print rx, tx }' /proc/net/dev
}

read -r rx1 tx1 <<< "$(read_bytes)"
sleep 1
read -r rx2 tx2 <<< "$(read_bytes)"

down=$(( (rx2 - rx1) / 1024 ))
up=$(( (tx2 - tx1) / 1024 ))
[ "$down" -lt 0 ] && down=0
[ "$up" -lt 0 ] && up=0

printf "\xe2\x86\x93%dKB/s \xe2\x86\x91%dKB/s" "$down" "$up"
