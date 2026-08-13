"""bubbles_layer.py — floating bubble physics layer for the cyberdeck.

A collection of soft, translucent bubbles that float around the background.
Bubbles repel each other softly (inverse-square), have gentle random drift,
and are tethered to weak anchor points via spring force — so they drift
around organically but never leave the screen entirely.

Layer ordering in paintEvent (bottom → top):
  1. flat background colour (from parent widget)
  2. pipes_layer — low-alpha ASCII texture
  3. bubbles_layer — translucent floating bubbles
  4. panel content — graph image, clock, stats, etc.

Use by creating one BubblesLayer per panel (or one shared across panels)
and calling `.paint(painter)` before drawing panel content.
"""
import math
import random
import time
from PySide6.QtCore import QObject, QTimer, QPointF, QRectF
from PySide6.QtGui import QPainter, QColor, QLinearGradient, Qt

# Bubble colour palette — One Dark–family cyans/blues/teals with occasional
# warm accent. These are drawn with high translucency so they blend softly
# into the existing background rather than dominating it.
BUBBLE_HUES = [198, 205, 215, 175, 220, 160]   # cyan → blue range
ACCENT_HUES = [280, 35]                          # purple, amber accents


def _hsla(h, s, l, a):
    """Return a QColor in One Dark–friendly HSLA space."""
    return QColor.fromHsl(h, s, l, a)


class _Bubble:
    """Single physics bubble. Coordinates are pixel-local to the owning
    widget; velocities are px/frame at 30 fps equivalents."""

    __slots__ = ('cx', 'cy', 'vx', 'vy', 'r', 'hue', 'mass',
                 'ax', 'ay', 'phase')

    def __init__(self, x, y, r, hue):
        self.cx = x
        self.cy = y
        self.vx = random.uniform(-0.15, 0.15)
        self.vy = random.uniform(-0.12, 0.12)
        self.r = r
        self.hue = hue
        self.mass = r * r  # bigger bubbles are heavier
        self.ax = 0.0
        self.ay = 0.0
        self.phase = random.uniform(0, 6.2832)  # for drift oscillation


class BubblesLayer(QObject):
    """Animated bubble field. Instantiate once per widget and call .paint()
    from that widget's paintEvent, before drawing content on top.

    Args:
        widget:      owning QWidget — used for size + repaint trigger
        count:       number of bubbles to simulate (default 18)
        min_r:       minimum bubble radius in px (default 14)
        max_r:       maximum bubble radius in px (default 55)
        interval_ms: timer interval in ms (default 33 ≈ 30 fps)
        repulsion:   force multiplier for bubble-bubble repulsion
        drift:       amplitude of gentle sinusoidal drift
        damping:     velocity damping per frame (0.98–1.0 recommended)
        edge_margin: px buffer from edges before soft wall bounce
        max_alpha:   max alpha for bubble fills (0–255, default 28)
    """

    def __init__(self, widget, *, count=18, min_r=14, max_r=55,
                 interval_ms=33, repulsion=0.8, drift=0.04,
                 damping=0.985, edge_margin=60, max_alpha=28):
        super().__init__(widget)
        self._widget = widget
        self.max_alpha = max_alpha
        self._drag = None  # current mouse drag target bubble, if any

        self._bubbles = []
        self._w = max(widget.width(), 400)
        self._h = max(widget.height(), 300)
        self._init_bubbles(count, min_r, max_r)

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(interval_ms)

        # Tuning constants
        self._repulsion = repulsion
        self._drift = drift
        self._damping = damping
        self._edge_margin = edge_margin
        self._spring_k = 0.0003   # weak tether to center-of-mass drift

    # ── Public API ────────────────────────────────────────────────────────

    def paint(self, p):
        """Draw all bubbles as the bottom-most animated layer. Call this
        at the top of the owning widget's paintEvent, before any content."""
        if not self._bubbles:
            return
        p.setRenderHint(QPainter.Antialiasing)
        p.setPen(Qt.NoPen)
        w, h = self._widget.width(), self._widget.height()
        for b in self._bubbles:
            self._draw_bubble(p, b, w, h)

    # ── Resize ────────────────────────────────────────────────────────────

    def on_resize(self, w, h):
        """Call when the owning widget resizes — clamp bubbles inward."""
        self._w = max(w, 100)
        self._h = max(h, 100)
        margin = self._edge_margin
        for b in self._bubbles:
            b.cx = max(b.r, min(self._w - b.r, b.cx))
            b.cy = max(b.r, min(self._h - b.r, b.cy))

    # ── Mouse interaction ─────────────────────────────────────────────────

    def mousePressEvent(self, event):
        pos = event.position() if hasattr(event, 'position') else event.localPos()
        wx, wy = pos.x(), pos.y()
        # Find nearest bubble within 30 px
        best, best_d = None, 30.0
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
        self._drag.vx *= 0.3
        self._drag.vy *= 0.3

    def mouseReleaseEvent(self, event):
        self._drag = None

    # ── Physics ───────────────────────────────────────────────────────────

    def _tick(self):
        w, h = self._widget.width(), self._widget.height()
        if w != self._w or h != self._h:
            self.on_resize(w, h)

        bubs = self._bubbles
        n = len(bubs)

        # Reset accelerations
        for b in bubs:
            b.ax = 0.0
            b.ay = 0.0

        # Pairwise repulsion (softened inverse-square)
        for i in range(n):
            bi = bubs[i]
            for j in range(i + 1, n):
                bj = bubs[j]
                dx = bi.cx - bj.cx
                dy = bi.cy - bj.cy
                d2 = dx * dx + dy * dy
                min_d = bi.r + bj.r
                if d2 < min_d * min_d and d2 > 0.01:
                    d = math.sqrt(d2)
                    # Force ramps up sharply as bubbles approach overlap
                    pressure = (min_d - d) / min_d
                    force = self._repulsion * pressure * 0.5
                    fx = dx / d * force
                    fy = dy / d * force
                    inv_i = 1.0 / bi.mass
                    inv_j = 1.0 / bj.mass
                    bi.vx += fx * inv_i
                    bi.vy += fy * inv_i
                    bj.vx -= fx * inv_j
                    bj.vy -= fy * inv_j

        # Wall repulsion (soft boundary)
        m = self._edge_margin
        for b in bubs:
            if b.cx < b.r + m:
                b.ax += (m - (b.cx - b.r)) * 0.02
            elif b.cx > w - b.r - m:
                b.ax -= (b.cx - (w - b.r - m)) * 0.02
            if b.cy < b.r + m:
                b.ay += (m - (b.cy - b.r)) * 0.02
            elif b.cy > h - b.r - m:
                b.ay -= (b.cy - (h - b.r - m)) * 0.02

        # Weak centering drift (so bubbles don't cluster at edges)
        cx_w, cy_w = w * 0.5, h * 0.5
        for b in bubs:
            b.ax += (cx_w - b.cx) * self._spring_k
            b.ay += (cy_w - b.cy) * self._spring_k

        # Gentle sinusoidal drift (organic feel)
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
            # Speed cap
            speed = math.hypot(b.vx, b.vy)
            if speed > 1.5:
                b.vx = b.vx / speed * 1.5
                b.vy = b.vy / speed * 1.5
            b.cx += b.vx
            b.cy += b.vy
            # Hard clamp (safety net)
            b.cx = max(b.r, min(w - b.r, b.cx))
            b.cy = max(b.r, min(h - b.r, b.cy))

        self._widget.update()

    # ── Drawing ───────────────────────────────────────────────────────────

    def _draw_bubble(self, p, b, w, h):
        r = int(b.r)
        x = int(b.cx)
        y = int(b.cy)

        # Base fill: very translucent hue
        base_a = int(self.max_alpha * 0.5)
        p.setBrush(_hsla(b.hue, 45, 55, base_a))
        p.drawEllipse(x - r, y - r, r * 2, r * 2)

        # Specular highlight (offset top-left)
        hl_r = max(1, r // 3)
        hx = x - r // 3
        hy = y - r // 2 - hl_r
        hl_a = int(self.max_alpha * 0.7)
        p.setBrush(_hsla(b.hue, 20, 88, hl_a))
        p.drawEllipse(hx - hl_r, hy - hl_r, hl_r * 2, hl_r * 2)

        # Subtle rim
        rim_a = int(self.max_alpha * 0.35)
        p.setPen(QColor.fromHsl(b.hue, 50, 65, rim_a))
        p.drawEllipse(x - r, y - r, r * 2, r * 2)

    # ── Init ──────────────────────────────────────────────────────────────

    def _init_bubbles(self, count, min_r, max_r):
        self._bubbles.clear()
        for _ in range(count):
            r = random.uniform(min_r, max_r)
            x = random.uniform(r + self._edge_margin,
                               max(r + self._edge_margin, self._w - r - self._edge_margin))
            y = random.uniform(r + self._edge_margin,
                               max(r + self._edge_margin, self._h - r - self._edge_margin))
            hue = random.choice(BUBBLE_HUES + ([random.choice(ACCENT_HUES)]
                                                if random.random() < 0.15 else []))
            self._bubbles.append(_Bubble(x, y, r, hue))
