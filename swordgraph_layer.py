"""swordgraph_layer.py — animated graph-bubble physics layer for the cyberdeck.

Each node in graph.json becomes a physical bubble; edges become elastic
strings pulling neighbouring bubbles together. Bubbles collide softly
(inverse-square repulsion), drift sinusoidally, and are gently tethered
toward the centre so nothing escapes. Clicking a bubble drags it;
releasing launches it with the residual velocity.

Drawn as an overlay on top of the graph image area inside main_panel.
"""
import math
import os
import json
import random
import time
from PySide6.QtCore import QObject, QTimer, Qt
from PySide6.QtGui import QPainter, QColor, QFont, QRadialGradient

# ── Palette (One Dark family) ────────────────────────────────────────────────
HUE_DONE        = 145   # green
HUE_IN_PROGRESS = 42    # amber
HUE_BLOCKED     = 358   # red
HUE_TODO        = 210   # blue

FILL_HUES = {
    "done":        HUE_DONE,
    "in-progress": HUE_IN_PROGRESS,
    "blocked":     HUE_BLOCKED,
}

CLUSTER_HUES = [198, 215, 175, 220, 160, 280, 35]   # cyan→blue→teal→purple→amber
STRING_MAX_D = 600                                   # px — connections reach far


def _status_hue(status: str) -> int:
    return FILL_HUES.get(status.strip().lower(), HUE_TODO)


# ── Internal bubble ──────────────────────────────────────────────────────────
class _Bubble:
    __slots__ = ('nid', 'label', 'cx', 'cy', 'vx', 'vy', 'r', 'fill_hue',
                 'mass', 'ax', 'ay', 'phase')

    def __init__(self, nid: str, label: str, x: float, y: float, r: float,
                 fill_hue: int):
        self.nid       = nid
        self.label     = label
        self.cx, self.cy = x, y
        self.vx, self.vy = 0.0, 0.0          # zero initial velocity
        self.r         = r
        self.fill_hue  = fill_hue
        self.mass      = r * r
        self.ax, self.ay = 0.0, 0.0
        self.phase     = random.uniform(0, 6.2832)


# ── Public layer ─────────────────────────────────────────────────────────────
class SwordGraphLayer(QObject):
    """Animated graph-bubble physics layer overlaid on the graph area."""

    def __init__(self, widget, *,
                 graph_json=None,
                 count=10,
                 min_r_pct=0.06, max_r_pct=0.18,
                 interval_ms=22, repulsion=6.0, drift=0.04,
                 damping=0.93, edge_margin=50,
                 string_max_d=STRING_MAX_D, max_alpha=72):
        super().__init__(widget)
        self._widget           = widget
        self.max_alpha         = max_alpha
        self._drag             = None
        self._string_max_d     = string_max_d
        self._min_r_pct        = min_r_pct
        self._max_r_pct        = max_r_pct

        # Graph data
        if graph_json is None:
            graph_json = os.path.join(
                os.path.expanduser("~/.config/animated-wallpaper"), "graph.json")
        self._graph_json   = graph_json
        self._edges        = []
        self._node_data    = []
        self._w            = max(widget.width(),  600)
        self._h            = max(widget.height(), 400)
        self._bubbles      = []

        # Tuning constants
        self._repulsion   = repulsion    # strong → pushes bubbles well apart
        self._drift       = drift
        self._damping     = damping      # lower → keeps momentum, less sticky
        self._edge_margin = edge_margin
        self._spring_k    = 0.00012
        self._node_count  = 0
        self._string_k    = 0.0008
        self._min_sep     = 8.0

        # Pre-warm physics so bubbles start well-spread (not clumped)
        self._warmup_ticks = 80            # ~1.8 s of pure-repulsion settling
        self._warmup_running = True

        # Compute radii from widget size (proportional, not fixed)
        self._base_r = max(int(min(self._w, self._h) * self._min_r_pct), 26)
        self._max_r  = max(int(min(self._w, self._h) * self._max_r_pct), 65)

        self._load_graph()
        self._init_bubbles(count)
        self._warmup()                      # spread bubbles before showing them

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(interval_ms)

    # ── Public API ─────────────────────────────────────────────────────────

    def paint(self, p: QPainter):
        if not self._bubbles:
            return
        p.setRenderHint(QPainter.Antialiasing)
        bubs = self._bubbles

        # ── Strings ─────────────────────────────────────────────────────
        for i, bi in enumerate(bubs):
            for bj in bubs[i + 1:]:
                d = math.hypot(bi.cx - bj.cx, bi.cy - bj.cy)
                if d < self._string_max_d:
                    alpha = int(self.max_alpha * 0.50 * (1.0 - d / self._string_max_d))
                    mid_hue = (bi.fill_hue + bj.fill_hue) // 2
                    p.setPen(QColor.fromHsl(mid_hue, 55, 65, alpha))
                    p.setBrush(Qt.NoBrush)
                    p.drawLine(int(bi.cx), int(bi.cy), int(bj.cx), int(bj.cy))

        # ── Bubbles ─────────────────────────────────────────────────────
        p.setPen(Qt.NoPen)
        for b in bubs:
            self._draw_bubble(p, b)

    def on_resize(self, w: int, h: int):
        self._w = max(w, 100)
        self._h = max(h, 100)
        m = self._edge_margin
        for b in self._bubbles:
            b.cx = max(b.r, min(self._w - b.r, b.cx))
            b.cy = max(b.r, min(self._h - b.r, b.cy))
        self._string_max_d = min(w, h) * 0.42
        self._base_r = max(int(min(self._w, self._h) * self._min_r_pct), 26)
        self._max_r  = max(int(min(self._w, self._h) * self._max_r_pct), 65)

    def mousePressEvent(self, event):
        pos = event.position() if hasattr(event, 'position') else event.localPos()
        wx, wy = pos.x(), pos.y()
        best, best_d = None, 50.0
        for b in self._bubbles:
            d = math.hypot(b.cx - wx, b.cy - wy)
            if d < best_d:
                best, best_d = b, d
        if best is not None:
            self._drag = best

    def mouseMoveEvent(self, event):
        if self._drag is None:
            return
        pos = event.position() if hasattr(event, 'position') else event.localPos()
        self._drag.cx = pos.x()
        self._drag.cy = pos.y()
        self._drag.vx *= 0.20
        self._drag.vy *= 0.20

    def mouseReleaseEvent(self, event):
        self._drag = None

    def sync_node_count(self, n: int):
        self._node_count = n

    def is_empty(self) -> bool:
        return len(self._bubbles) == 0

    # ── Graph loading ──────────────────────────────────────────────────────

    def _load_graph(self):
        """Read graph.json → populate _node_data and _edges."""
        self._node_data.clear()
        self._edges.clear()
        try:
            with open(self._graph_json) as f:
                d = json.load(f)
            # Skip phantom 'source'/'target' nodes produced by swordgraph's
            # tree scan (they have no real label/path and are used only as
            # internal DFS anchors — not meant to be visualised).
            valid_ids = set()
            for n in d.get("nodes", []):
                nid = n.get("id", "")
                # A real node must have a meaningful label (not just 'source'/'target')
                label = n.get("label", "").strip()
                if label and nid not in ("source", "target") and len(label) > 1:
                    self._node_data.append((
                        nid, label, n.get("status", ""),
                        _status_hue(n.get("status", ""))
                    ))
                    valid_ids.add(nid)
            id_map = {n[0]: idx for idx, n in enumerate(self._node_data)}
            for e in d.get("edges", []):
                a = e.get("from") or e.get("source")
                b = e.get("to")   or e.get("target")
                if a is not None and b is not None and a in valid_ids and b in valid_ids:
                    self._edges.append((id_map[a], id_map[b]))
        except Exception:
            pass

    # ── Bubble initialisation + warmup ──────────────────────────────────────

    def _init_bubbles(self, count: int):
        """Place bubbles on a grid, then run warmup physics to spread them."""
        self._load_graph()
        self._bubbles.clear()
        w, h = self._w, self._h
        margin = self._edge_margin

        if self._node_data:
            # Degree-aware sizing: high-degree nodes get slightly bigger
            degree = {}
            for a, b in self._edges:
                degree[a] = degree.get(a, 0) + 1
                degree[b] = degree.get(b, 0) + 1
            max_deg = max(degree.values(), default=1)

            # Grid layout: compute cols × rows to fill the area
            n_nodes = len(self._node_data)
            aspect  = w / h
            cols = max(1, round(math.sqrt(n_nodes * aspect)))
            rows = max(1, math.ceil(n_nodes / cols))
            cell_w = (w - 2 * margin) / cols
            cell_h = (h - 2 * margin) / rows
            cell_size = min(cell_w, cell_h)

            for idx, (nid, label, status, hue) in enumerate(self._node_data):
                deg   = degree.get(idx, 0) + 1
                # Radius: scale with degree but clamp to [base_r, max_r]
                r = self._base_r + (self._max_r - self._base_r) * (deg / max_deg)
                r = max(self._base_r, min(self._max_r, r))
                # Place on grid with small random jitter
                col = idx % cols
                row = idx // cols
                cx = margin + r + col * cell_w + random.uniform(-cell_w * 0.15, cell_w * 0.15)
                cy = margin + r + row * cell_h + random.uniform(-cell_h * 0.15, cell_h * 0.15)
                cx = max(r, min(w - r, cx))
                cy = max(r, min(h - r, cy))
                self._bubbles.append(_Bubble(nid, label, cx, cy, r, hue))
        else:
            # Fallback: scatter
            for _ in range(count):
                r  = random.uniform(self._base_r, self._max_r)
                cx = random.uniform(r + margin, max(r + margin, w - r - margin))
                cy = random.uniform(r + margin, max(r + margin, h - r - margin))
                hue = random.choice(CLUSTER_HUES)
                self._bubbles.append(_Bubble(f"node_{_}", f"node{_}", cx, cy, r, hue))

    def _warmup(self):
        """Run pure-repulsion physics steps off-screen so bubbles spread out
        before any frame is drawn. No strings, no drift — just collision
        resolution until no overlaps remain."""
        w, h = self._w, self._h
        bubs = self._bubbles
        n    = len(bubs)
        if n <= 1:
            return

        for _ in range(self._warmup_ticks):
            for b in bubs:
                b.ax = 0.0
                b.ay = 0.0

            # Repulsion only (no strings during warmup)
            for i in range(n):
                bi = bubs[i]
                for j in range(i + 1, n):
                    bj = bubs[j]
                    dx = bi.cx - bj.cx
                    dy = bi.cy - bj.cy
                    d2 = dx * dx + dy * dy
                    min_d = bi.r + bj.r + self._min_sep
                    if d2 < min_d * min_d and d2 > 0.001:
                        d = math.sqrt(d2)
                        pressure = (min_d - d) / min_d
                        force = self._repulsion * pressure * 0.7
                        fx = dx / d * force
                        fy = dy / d * force
                        ii, jj = 1.0 / bi.mass, 1.0 / bj.mass
                        bi.vx += fx * ii; bi.vy += fy * ii
                        bj.vx -= fx * jj; bj.vy -= fy * jj

            # Wall repulsion
            m = self._edge_margin
            for b in bubs:
                if b.cx < b.r + m:   b.ax += (m - (b.cx - b.r)) * 0.05
                elif b.cx > w - b.r - m: b.ax -= (b.cx - (w - b.r - m)) * 0.05
                if b.cy < b.r + m:   b.ay += (m - (b.cy - b.r)) * 0.05
                elif b.cy > h - b.r - m: b.ay -= (b.cy - (h - b.r - m)) * 0.05

            # Integrate
            for b in bubs:
                b.vx *= 0.90; b.vy *= 0.90          # heavy damping during warmup
                b.vx += b.ax; b.vy += b.ay
                b.cx += b.vx; b.cy += b.vy
                b.cx = max(b.r, min(w - b.r, b.cx))
                b.cy = max(b.r, min(h - b.r, b.cy))

            # Hard separation pass (guarantee no overlaps)
            for i in range(n):
                bi = bubs[i]
                for j in range(i + 1, n):
                    bj = bubs[j]
                    dx = bj.cx - bi.cx
                    dy = bj.cy - bi.cy
                    d  = math.hypot(dx, dy)
                    min_dist = bi.r + bj.r + self._min_sep
                    if d < min_dist and d > 0.001:
                        push = (min_dist - d) * 0.5
                        nx, ny = dx / d, dy / d
                        ii, jj = 1.0 / bi.mass, 1.0 / bj.mass
                        bi.cx -= nx * push * ii
                        bi.cy -= ny * push * ii
                        bj.cx += nx * push * jj
                        bj.cy += ny * push * jj
                        # Clamp
                        bi.cx = max(bi.r, min(w - bi.r, bi.cx))
                        bi.cy = max(bi.r, min(h - bi.r, bi.cy))
                        bj.cx = max(bj.r, min(w - bj.r, bj.cx))
                        bj.cy = max(bj.r, min(h - bj.r, bj.cy))

        self._warmup_running = False

    # ── Live physics tick ────────────────────────────────────────────────────

    def _tick(self):
        w, h = self._widget.width(), self._widget.height()
        if w != self._w or h != self._h:
            self.on_resize(w, h)

        bubs = self._bubbles
        n    = len(bubs)
        if n == 0:
            return

        for b in bubs:
            b.ax = 0.0
            b.ay = 0.0

        # Pairwise repulsion
        repulse_range = self._string_max_d
        for i in range(n):
            bi = bubs[i]
            for j in range(i + 1, n):
                bj = bubs[j]
                dx = bi.cx - bj.cx
                dy = bi.cy - bj.cy
                if abs(dx) > repulse_range or abs(dy) > repulse_range:
                    continue
                d2 = dx * dx + dy * dy
                min_d = bi.r + bj.r
                if d2 < min_d * min_d and d2 > 0.01:
                    d = math.sqrt(d2)
                    pressure = (min_d - d) / min_d
                    force = self._repulsion * pressure * 0.6
                    fx = dx / d * force
                    fy = dy / d * force
                    ii, jj = 1.0 / bi.mass, 1.0 / bj.mass
                    bi.vx += fx * ii; bi.vy += fy * ii
                    bj.vx -= fx * jj; bj.vy -= fy * jj

        # String tension (only after warmup; during warmup only repulsion runs)
        if not self._warmup_running:
            for ei, ej in self._edges:
                if ei >= n or ej >= n:
                    continue
                bi, bj = bubs[ei], bubs[ej]
                dx = bj.cx - bi.cx
                dy = bj.cy - bi.cy
                d  = math.hypot(dx, dy)
                if d < self._string_max_d and d > 1.0:
                    rest_length = (bi.r + bj.r) * 3.0
                    stretch = max(0.0, d - rest_length)
                    spring_force = stretch * self._string_k
                    fx = dx / d * spring_force
                    fy = dy / d * spring_force
                    bi.vx += fx / bi.mass; bi.vy += fy / bi.mass
                    bj.vx -= fx / bj.mass; bj.vy -= fy / bj.mass

        # Wall repulsion
        m = self._edge_margin
        for b in bubs:
            if b.cx < b.r + m:
                b.ax += (m - (b.cx - b.r)) * 0.020
            elif b.cx > w - b.r - m:
                b.ax -= (b.cx - (w - b.r - m)) * 0.020
            if b.cy < b.r + m:
                b.ay += (m - (b.cy - b.r)) * 0.020
            elif b.cy > h - b.r - m:
                b.ay -= (b.cy - (h - b.r - m)) * 0.020

        # Weak centre tether
        cx_w, cy_w = w * 0.5, h * 0.5
        for b in bubs:
            b.ax += (cx_w - b.cx) * self._spring_k
            b.ay += (cy_w - b.cy) * self._spring_k

        # Sinusoidal drift
        ft = time.time()
        for b in bubs:
            b.ax += math.sin(ft * 0.7 + b.phase) * self._drift
            b.ay += math.cos(ft * 0.5 + b.phase * 1.3) * self._drift

        # Integrate
        for b in bubs:
            if b is self._drag:
                continue
            b.vx = b.vx * self._damping + b.ax
            b.vy = b.vy * self._damping + b.ay
            speed = math.hypot(b.vx, b.vy)
            if speed > 2.2:
                b.vx = b.vx / speed * 2.2
                b.vy = b.vy / speed * 2.2
            b.cx += b.vx
            b.cy += b.vy
            b.cx = max(b.r, min(w - b.r, b.cx))
            b.cy = max(b.r, min(h - b.r, b.cy))

        # Post-integration hard separation (prevents any accidental overlap)
        for i in range(n):
            bi = bubs[i]
            for j in range(i + 1, n):
                bj = bubs[j]
                dx = bj.cx - bi.cx
                dy = bj.cy - bi.cy
                d  = math.hypot(dx, dy)
                min_dist = bi.r + bj.r + self._min_sep
                if d < min_dist and d > 0.01:
                    push = (min_dist - d) * 0.5
                    nx, ny = dx / d, dy / d
                    ii, jj = 1.0 / bi.mass, 1.0 / bj.mass
                    bi.cx -= nx * push * ii
                    bi.cy -= ny * push * ii
                    bj.cx += nx * push * jj
                    bj.cy += ny * push * jj
                    bi.cx = max(bi.r, min(w - bi.r, bi.cx))
                    bi.cy = max(bi.r, min(h - bi.r, bi.cy))
                    bj.cx = max(bj.r, min(w - bj.r, bj.cx))
                    bj.cy = max(bj.r, min(h - bj.r, bj.cy))
                    # Dampen relative velocity along collision normal
                    rel_vx = bi.vx - bj.vx
                    rel_vy = bi.vy - bj.vy
                    dot = rel_vx * nx + rel_vy * ny
                    if dot > 0:
                        bi.vx -= nx * dot * 0.25
                        bi.vy -= ny * dot * 0.25
                        bj.vx += nx * dot * 0.25
                        bj.vy += ny * dot * 0.25

        self._widget.update()

    # ── Drawing ─────────────────────────────────────────────────────────────

    def _draw_bubble(self, p: QPainter, b: _Bubble):
        r   = int(b.r)
        x   = int(b.cx)
        y   = int(b.cy)
        hue = b.fill_hue
        ma  = self.max_alpha

        # Outer glow halo
        glow_r = int(r * 1.4)
        halo = QRadialGradient(x, y, glow_r)
        halo.setColorAt(0.0,  QColor.fromHsl(hue, 60, 60, int(ma * 0.12)))
        halo.setColorAt(0.6,  QColor.fromHsl(hue, 50, 50, int(ma * 0.04)))
        halo.setColorAt(1.0,  QColor.fromHsl(hue, 40, 40, 0))
        p.setBrush(halo); p.setPen(Qt.NoPen)
        p.drawEllipse(x - glow_r, y - glow_r, glow_r * 2, glow_r * 2)

        # Radial gradient fill
        grad = QRadialGradient(x - r // 4, y - r // 3, r * 1.2)
        grad.setColorAt(0.0,  QColor.fromHsl(hue, 55, 65, int(ma * 0.85)))
        grad.setColorAt(0.45, QColor.fromHsl(hue, 50, 55, int(ma * 0.65)))
        grad.setColorAt(0.80, QColor.fromHsl(hue, 45, 42, int(ma * 0.35)))
        grad.setColorAt(1.0,  QColor.fromHsl(hue, 40, 35, 0))
        p.setBrush(grad); p.setPen(Qt.NoPen)
        p.drawEllipse(x - r, y - r, r * 2, r * 2)

        # Specular highlight
        hl_r = max(3, r // 3)
        hx, hy = x - r // 3, y - r // 2 - hl_r
        hl = QRadialGradient(hx, hy, hl_r * 1.6)
        hl.setColorAt(0, QColor.fromHsl(hue, 10, 92, int(ma * 0.75)))
        hl.setColorAt(1, QColor.fromHsl(hue, 10, 92, 0))
        p.setBrush(hl); p.setPen(Qt.NoPen)
        p.drawEllipse(hx - hl_r, hy - hl_r, hl_r * 2, hl_r * 2)

        # Rim
        rim_a = int(ma * 0.55)
        p.setPen(QColor.fromHsl(hue, 55, 68, rim_a)); p.setBrush(Qt.NoBrush)
        p.drawEllipse(x - r, y - r, r * 2, r * 2)

        # Label — font scales with bubble radius, text area wide enough for full label
        font_size = max(8, min(14, int(r * 0.22)))
        p.setPen(QColor.fromHsl(hue, 15, 93, 245))
        p.setFont(QFont("JetBrains Mono", font_size))
        metrics = p.fontMetrics()
        # Cell width = bubble diameter * 0.85 → leaves room for glow, never elides
        cell_w = int(r * 1.6)
        cell_h = int(metrics.height() * 2.0)   # two lines of headroom
        text_y = y - cell_h // 2 + 1

        label = b.label
        if metrics.horizontalAdvance(label) > cell_w:
            # Split at last space before midpoint
            mid = len(label) // 2
            split = label.rfind(' ', 0, mid + 4)
            if split <= 0:
                split = mid
            line1 = label[:split].rstrip()
            line2 = label[split:].lstrip()
            p.drawText(x - r, text_y, r * 2, int(metrics.height()),
                       Qt.AlignHCenter | Qt.AlignTop, line1)
            p.drawText(x - r, text_y + int(metrics.height()) + 2, r * 2,
                       int(metrics.height()),
                       Qt.AlignHCenter | Qt.AlignTop, line2)
        else:
            p.drawText(x - r, text_y, r * 2, cell_h,
                       Qt.AlignHCenter | Qt.AlignVCenter, label)

        # Status dot beneath label
        dot_r = max(2, r // 8)
        dot_y = text_y + cell_h + 1
        if dot_y < y + r:
            p.setBrush(QColor.fromHsl(hue, 75, 68, int(ma * 0.95)))
            p.setPen(Qt.NoPen)
            p.drawEllipse(x - dot_r, dot_y - dot_r, dot_r * 2, dot_r * 2)
