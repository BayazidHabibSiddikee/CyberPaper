#!/bin/bash
# graph-render.sh — renders ~/.config/animated-wallpaper/graph.json → graph.png
# Uses jq to emit Graphviz dot, then neato for force-directed layout.
# Args: $1=width_px $2=height_px  (optional, default 845×1026)

set -euo pipefail

CFG_DIR="$HOME/.config/animated-wallpaper"
GRAPH_JSON="$CFG_DIR/graph.json"
GRAPH_DOT="$CFG_DIR/graph.dot"
GRAPH_PNG="$CFG_DIR/graph.png"

WIDTH_PX="${1:-845}"
HEIGHT_PX="${2:-1026}"

# Convert px → inches at ~100 dpi for neato's -Gsize
WIDTH_IN=$(awk "BEGIN{printf \"%.2f\", $WIDTH_PX/100}")
HEIGHT_IN=$(awk "BEGIN{printf \"%.2f\", $HEIGHT_PX/100}")

mkdir -p "$CFG_DIR"
[ -f "$GRAPH_JSON" ] || cp "$(dirname "$0")/graph.default.json" "$GRAPH_JSON"

jq -r '
  def color(status):
    if status == "done"        then "#98c379"
    elif status == "in-progress" then "#e5c07b"
    elif status == "blocked"   then "#e06c75"
    else "#61afef" end;

  def fillcolor(status):
    if status == "done"        then "#98c37925"
    elif status == "in-progress" then "#e5c07b25"
    elif status == "blocked"   then "#e06c7525"
    else "#61afef25" end;

  "digraph G {",
  "  layout=neato;",
  "  overlap=false;",
  "  splines=ortho;",
  "  bgcolor=\"transparent\";",
  "  node [shape=box style=\"filled,rounded\" fontname=\"JetBrainsMono Nerd Font\" fontsize=11",
  "        fontcolor=\"#abb2bf\" penwidth=1.5 margin=0.15",
  "        fillcolor=\"#3e445180\" color=\"#3e4451\"];",
  "  edge [color=\"#3e445188\" penwidth=1.2 arrowsize=0.6 arrowhead=vee];",
  (.nodes[] | "  \"\(.id)\" [label=\"\(.label)\", fillcolor=\"\(fillcolor(.status))\", color=\"\(color(.status))\"];"),
  (if .edges then (.edges[] | "  \"\(.from)\" -> \"\(.to)\";") else empty end),
  "}"
' "$GRAPH_JSON" > "$GRAPH_DOT"

neato -Tpng \
    -Gsize="${WIDTH_IN},${HEIGHT_IN}\!" \
    -Gdpi=100 \
    "$GRAPH_DOT" -o "$GRAPH_PNG"

echo "graph rendered → $GRAPH_PNG (${WIDTH_PX}×${HEIGHT_PX}px)"
