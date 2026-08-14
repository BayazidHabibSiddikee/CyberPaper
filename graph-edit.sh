#!/bin/bash
# graph-edit.sh — keyboard-driven mission graph editor using rofi + jq.
# Bind this to a key (e.g. $mod+Ctrl+e) instead of clicking on the wallpaper,
# since the wallpaper itself is click-through by design.

set -euo pipefail

CFG_DIR="$HOME/.config/animated-wallpaper"
GRAPH_JSON="$CFG_DIR/graph.json"
HERE="$(cd "$(dirname "$0")" && pwd)"

mkdir -p "$CFG_DIR"
[ -f "$GRAPH_JSON" ] || cp "$HERE/graph.default.json" "$GRAPH_JSON"

notify() { command -v notify-send >/dev/null && notify-send "Cyberdesk" "$1" || true; }
refresh() { "$HERE/graph-render.sh" >/dev/null 2>&1 || true; }

pick_node() {  # prints chosen node id, or empty if cancelled
  local prompt="${1:-Task}"
  local sel
  sel=$(jq -r '.nodes[] | "\(.id)\t\(.label) [\(.status)]"' "$GRAPH_JSON" \
        | rofi -dmenu -i -p "$prompt" -format i)
  [ -z "${sel:-}" ] && return 1
  jq -r ".nodes[$sel].id" "$GRAPH_JSON"
}

add_task() {
  local label status id tmp
  label=$(rofi -dmenu -p "New task label:" </dev/null)
  [ -z "$label" ] && return
  status=$(printf 'todo\nin-progress\ndone\nblocked\n' | rofi -dmenu -p "Status:")
  [ -z "$status" ] && status="todo"
  id="task_$(date +%s%N)"
  tmp=$(mktemp)
  jq --arg id "$id" --arg label "$label" --arg status "$status" \
     '.nodes += [{"id": $id, "label": $label, "status": $status}]' \
     "$GRAPH_JSON" > "$tmp" && mv "$tmp" "$GRAPH_JSON"
  notify "Added: $label"
  refresh
}

link_tasks() {
  local from to tmp
  from=$(pick_node "Arrow FROM:") || return
  to=$(pick_node "Arrow TO:") || return
  [ "$from" = "$to" ] && { notify "Can't link a task to itself"; return; }
  tmp=$(mktemp)
  jq --arg from "$from" --arg to "$to" \
     '.edges += [{"from": $from, "to": $to}]' \
     "$GRAPH_JSON" > "$tmp" && mv "$tmp" "$GRAPH_JSON"
  notify "Linked $from -> $to"
  refresh
}

set_status() {
  local id status tmp
  id=$(pick_node "Set status of:") || return
  status=$(printf 'todo\nin-progress\ndone\nblocked\n' | rofi -dmenu -p "New status:")
  [ -z "$status" ] && return
  tmp=$(mktemp)
  jq --arg id "$id" --arg status "$status" \
     '(.nodes[] | select(.id == $id) | .status) = $status' \
     "$GRAPH_JSON" > "$tmp" && mv "$tmp" "$GRAPH_JSON"
  notify "Status updated"
  refresh
}

rename_task() {
  local id label tmp
  id=$(pick_node "Rename:") || return
  label=$(rofi -dmenu -p "New label:" </dev/null)
  [ -z "$label" ] && return
  tmp=$(mktemp)
  jq --arg id "$id" --arg label "$label" \
     '(.nodes[] | select(.id == $id) | .label) = $label' \
     "$GRAPH_JSON" > "$tmp" && mv "$tmp" "$GRAPH_JSON"
  refresh
}

delete_task() {
  local id tmp
  id=$(pick_node "Delete:") || return
  tmp=$(mktemp)
  jq --arg id "$id" \
     '.nodes |= map(select(.id != $id)) |
      .edges |= map(select(.from != $id and .to != $id))' \
     "$GRAPH_JSON" > "$tmp" && mv "$tmp" "$GRAPH_JSON"
  notify "Deleted"
  refresh
}

main_menu() {
  local choice
  choice=$(printf 'Add Task\nLink Tasks (draw arrow)\nSet Status\nRename Task\nDelete Task\n' \
           | rofi -dmenu -i -p "Mission Graph")
  case "$choice" in
    "Add Task") add_task ;;
    "Link Tasks (draw arrow)") link_tasks ;;
    "Set Status") set_status ;;
    "Rename Task") rename_task ;;
    "Delete Task") delete_task ;;
    *) exit 0 ;;
  esac
}

main_menu
