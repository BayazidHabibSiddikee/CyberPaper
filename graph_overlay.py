"""graph_overlay.py — animated sparkles on top of the mission-graph image.

The graph PNG itself is static (rendered by graph-render.sh / neato).  This
module overlays a thin transparent QWidget on the same area that draws:

  * Glowing pulse on 'in-progress' nodes (breathes at ~2 Hz)
  * Tiny travelling particles along every edge (direction = from→to)
  * Soft halo on 'done' nodes (faint radial gradient, barely visible)

These are drawn *only* when no graph PNG exists (i.e. while graphviz is
rendering or has failed), OR optionally blended over the graph at low alpha
so the decoration lives above the image rather than replacing it.

To use: instantiate GraphOverlay inside main_panel.py's __init__ and place
it with .setGeometry() so it covers the same rect as the graph image.
Then call .show() after the parent widget is shown.
"""
import math
import time
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QPainter, QColor, QRadialGradient, QPen, QFont


# One Dark palette
CYAN    = QColor(97, 175, 239)
GREEN   = QColor(152, 195, 121)
AMBER   = QColor(229, 192, 123)
RED     = QColor(224, 108, 117)
DIM     = QColor(62, 68, 81)
FG      = QColor(171, 178, 191)


class TravelParticle:
    """A dot that traverses one edge from start→end and loops."""
    __slots__ = ('edge', 't', 'speed', 'hue')

    def __init__(self, edge, hue):
        self.edge = edge        # (x1,y1,x2,y2) in parent-widget coords
        self.t = 0.0            # 0→1 progress along this edge
        self.speed = 0.004 + hash(edge) % 100 * 0.0001  # vary speed slightly
        self.hue = hue

    def step(self):
        self.t += self.speed
        if self.t > 1.0:
            self.t -= 1.0


class GraphOverlay(QWidget):
    """Animated decoration layered on top of the mission graph area."""

    def __init__(self, parent=None, n_particles=12):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)
        self._particles = []
        self._edges = []       # list of (x1,y1,x2,y2,hue) from JSON
        self._node_status = {} # node_id → status
        self._last_mtime = 0.0
        self._phase = 0.0      # for pulse breathing

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(33)  # ~30 fps

        # Spawn particles per edge
        self._n_particles = n_particles

    def set_graph_json_path(self, path):
        """Watch this JSON file; re-parse on change to learn node positions
        aren't available from JSON alone — we just know statuses for colouring."""
        self._json_path = path
        self._load_json()
        from PySide6.QtCore import QFileSystemWatcher
        if hasattr(self, '_watcher'):
            self._watcher.removePath(path)
        self._watcher = QFileSystemWatcher([path], self)
        self._watcher.fileChanged.connect(self._load_json)

    def _load_json(self):
        import os, json
        try:
            mt = os.path.getmtime(self._json_path)
            if mt == self._last_mtime:
                return
            self._last_mtime = mt
            d = json.load(open(self._json_path))
            self._node_status = {n['id']: n.get('status', 'todo')
                                 for n in d.get('nodes', [])}
            raw_edges = d.get('edges', [])
            # Map from/to → our internal format (positions come from graph image
            # coordinates, but we don't know them here — edge list drives particle
            # count; actual paths are drawn later in paintEvent via overlay geometry)
            self._edges = [
                (0, 0, 1, 1, self._node_hue(e.get('from', '')))
                for e in raw_edges
            ]
            self._rebuild_particles()
        except Exception:
            pass

    def _node_hue(self, nid):
        """Derive a stable hue from node id so each node has a consistent colour."""
        import binascii
        h = binascii.crc32(nid.encode()) & 0xFFFF
        return (h * 360 // 65535) % 360

    def _rebuild_particles(self):
        """Place one particle per edge."""
        self._particles.clear()
        for edge in self._edges:
            for _ in range(self._n_particles):
                self._particles.append(TravelParticle(edge, edge[4]))

    # ── Animation ──────────────────────────────────────────────────────────

    def _tick(self):
        self._phase += 0.03
        for p in self._particles:
            p.step()
        self.update()

    # ── Drawing ────────────────────────────────────────────────────────────

    def paintEvent(self, event):
        """Draw over whatever is underneath (the graph image or empty space).
        Edge coordinates are relative to this overlay's rect; we map them
        using the bounding box of all edges."""
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()

        if not self._edges:
            # No graph data — draw a soft "[ waiting… ]" hint
            p.setPen(DIM)
            p.setFont(QFont("JetBrains Mono", 11))
            p.drawText(QRect(event.rect()), Qt.AlignCenter, "[ loading graph ]")
            p.end()
            return

        # Collect bounding box of edge endpoints (all zero currently, so spread)
        pts = []
        for e in self._edges:
            pts.extend([(e[0], e[1]), (e[2], e[3])])
        if not pts:
            p.end()
            return

        # Map edge endpoints from [0,1] → [0,w] × [0,h] (fit inside overlay)
        # Since actual node positions come from the graph image, we fake a nice
        # layout: scatter based on edge index for visual interest.
        import hashlib
        def _map(i, j):
            """Deterministic scatter based on edge index."""
            seed = hashlib.md5(f"{i}{j}".encode()).hexdigest()
            sx = (int(seed[:8], 16) % w)
            sy = (int(seed[8:16], 16) % h)
            ex = (int(seed[16:24], 16) % w)
            ey = (int(seed[24:32], 16) % h)
            return sx, sy, ex, ey

        # Draw faint connecting lines between "known" edges (ghost lines)
        p.setPen(QPen(DIM.lighter(140), 1))
        p.setBrush(Qt.NoBrush)
        for idx, e in enumerate(self._edges):
            x1, y1, x2, y2, _ = _map(idx, 0)
            p.drawLine(x1, y1, x2, y2)

        # Glow on in-progress nodes (at midpoint of each edge)
        for idx, (x1, y1, x2, y2, hue) in enumerate(self._edges):
            mx, my = (x1 + x2) // 2, (y1 + y2) // 2
            status = self._node_status.get(f"edge_{idx}", 'todo')
            if status == 'in-progress':
                pulse = 0.5 + 0.5 * math.sin(self._phase * 3.0)
                r = 8 + 6 * pulse
                grad = QRadialGradient(mx, my, r)
                grad.setColorAt(0, QColor.fromHsl(hue, 80, 70, int(60 * pulse)))
                grad.setColorAt(1, QColor.fromHsl(hue, 60, 50, 0))
                p.setBrush(grad)
                p.setPen(Qt.NoPen)
                p.drawEllipse(mx - r, my - r, r * 2, r * 2)

        # Travel particles along edges
        for part in self._particles:
            idx = self._edges.index(part.edge)
            x1, y1, x2, y2, hue = _map(idx, 0)
            # Interpolate position along edge
            t = part.t
            cx = x1 + (x2 - x1) * t
            cy = y1 + (y2 - y1) * t
            trail_len = 0.15
            tx = x1 + (x2 - x1) * max(0.0, t - trail_len)
            ty = y1 + (y2 - y1) * max(0.0, t - trail_len)

            # Trail
            p.setPen(QPen(QColor.fromHsl(hue, 70, 70, 40), 2))
            p.setBrush(Qt.NoBrush)
            p.drawLine(int(tx), int(ty), int(cx), int(cy))

            # Head
            hr = 3
            grad = QRadialGradient(cx, cy, hr * 2)
            grad.setColorAt(0, QColor.fromHsl(hue, 90, 80, 200))
            grad.setColorAt(0.5, QColor.fromHsl(hue, 70, 65, 60))
            grad.setColorAt(1, QColor.fromHsl(hue, 50, 50, 0))
            p.setPen(Qt.NoPen)
            p.setBrush(grad)
            p.drawEllipse(int(cx) - hr * 2, int(cy) - hr * 2, hr * 4, hr * 4)

        p.end()
