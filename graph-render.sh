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
    if status == "done"        then "#50ff78"
    elif status == "in-progress" then "#ffc800"
    elif status == "blocked"   then "#ff465a"
    else "#00b4ff" end;

  def fillcolor(status):
    if status == "done"        then "#50ff7830"
    elif status == "in-progress" then "#ffc80030"
    elif status == "blocked"   then "#ff465a30"
    else "#00b4ff30" end;

  "digraph G {",
  "  layout=neato;",
  "  overlap=false;",
  "  splines=true;",
  "  bgcolor=\"transparent\";",
  "  node [shape=box style=\"filled,rounded\" fontname=\"monospace\" fontsize=12",
  "        fontcolor=\"#e6ebff\" penwidth=2 margin=0.2];",
  "  edge [color=\"#00c8ff88\" penwidth=1.8 arrowsize=0.8];",
  (.nodes[] | "  \"\(.id)\" [label=\"\(.label)\", fillcolor=\"\(fillcolor(.status))\", color=\"\(color(.status))\"];"),
  (if .edges then (.edges[] | "  \"\(.from)\" -> \"\(.to)\";") else empty end),
  "}"
' "$GRAPH_JSON" > "$GRAPH_DOT"

neato -Tpng \
    -Gsize="${WIDTH_IN},${HEIGHT_IN}\!" \
    -Gdpi=100 \
    "$GRAPH_DOT" -o "$GRAPH_PNG"

echo "graph rendered → $GRAPH_PNG (${WIDTH_PX}×${HEIGHT_PX}px)"
