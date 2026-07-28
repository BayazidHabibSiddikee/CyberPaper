"""
left_panel.py — continues the mission graph from the center panel, with
the animated pipes texture running underneath it.

No matrix rain here anymore — the pipes animation is the one texture
layer, and it now runs in both the left AND right panels (see
right_panel.py), always sitting below each panel's real content and
above the flat deck background painted by cyberdeck.py.
"""
import os
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import Qt, QTimer, QRect
from PySide6.QtGui import QPainter, QColor, QFont, QPixmap

from pipes_layer import PipesLayer

CFG_DIR    = os.path.expanduser("~/.config/animated-wallpaper")
GRAPH_PNG  = os.path.join(CFG_DIR, "graph.png")

DIM = QColor(26, 58, 90)


class LeftPanel(QWidget):
    """Shows the left-hand crop of the shared mission-graph image (the
    same file center_panel.py renders into), so the graph reads as one
    continuous picture spanning the left + center columns. crop_x/total_w
    are passed in by cyberdeck.py so this panel knows which slice of the
    combined (left+center width) graph image belongs to it."""

    def __init__(self, parent=None, crop_x=0, total_w=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self._crop_x = crop_x
        self._total_w = total_w   # set by cyberdeck.py once widths are known

        self._pipes = PipesLayer(self)

        self._px = None
        self._mtime = 0
        self._load_graph()

        t = QTimer(self)
        t.timeout.connect(self._check_graph)
        t.start(3000)

    def _load_graph(self):
        try:
            mt = os.path.getmtime(GRAPH_PNG)
            if mt != self._mtime:
                self._mtime = mt
                self._px = QPixmap(GRAPH_PNG)
        except Exception:
            self._px = None

    def _check_graph(self):
        try:
            if os.path.getmtime(GRAPH_PNG) != self._mtime:
                self._load_graph()
                self.update()
        except Exception:
            pass

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        # no fill — shared bluish background comes from the deck window

        # ── Bottom layer: animated pipes texture ────────────────────
        self._pipes.paint(p)

        # ── Top layer: this panel's slice of the shared mission graph ──
        if self._px and not self._px.isNull():
            # The image was rendered at (left width + center width), so
            # take the slice starting at crop_x — this lines up exactly
            # with where center_panel.py starts drawing its own slice.
            src = QRect(self._crop_x, 0, w, self._px.height())
            src = src.intersected(self._px.rect())
            p.drawPixmap(QRect(0, 0, w, h), self._px, src)
        else:
            p.setPen(DIM)
            p.setFont(QFont("JetBrains Mono", 11))
            p.drawText(QRect(0, 0, w, h), Qt.AlignCenter, "[ no graph ]")

        p.end()
