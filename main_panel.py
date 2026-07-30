"""main_panel.py — merged left+center panel.

This replaces left_panel.py + center_panel.py. Those two used to split a
single shared graph image with a crop_x offset so it would "read as one
picture" across the seam — but two independently-painted widgets drawing
their own slice never lines up perfectly (text clipped mid-word at the
column boundary, the telemetry strip only living in the center column,
etc). Now it's one widget spanning the full left+center width, so there
is no seam: one pixmap, one paintEvent, one telemetry strip running the
full width along the bottom.
"""
import os, json, subprocess
from datetime import datetime
from PySide6.QtWidgets import QWidget, QPushButton
from PySide6.QtCore import Qt, QTimer, QRect, QFileSystemWatcher
from PySide6.QtGui import QPainter, QColor, QFont, QPixmap, QFontMetrics, QPen

from pipes_layer import PipesLayer

CFG_DIR    = os.path.expanduser("~/.config/animated-wallpaper")
GRAPH_PNG  = os.path.join(CFG_DIR, "graph.png")
GRAPH_JSON = os.path.join(CFG_DIR, "graph.json")

CYAN  = QColor(97, 175, 239)    # One Dark blue
GREEN = QColor(152, 195, 121)   # One Dark green
DIM   = QColor(62, 68, 81)      # One Dark dark gray
WHITE = QColor(171, 178, 191)   # One Dark foreground


class MainPanel(QWidget):
    """Full-width panel spanning what used to be the left + center columns."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)

        self._pipes = PipesLayer(self)

        self._px    = None
        self._mtime = 0
        self._nodes = 0
        self._edges = 0
        self._load_graph()

        # Clock: update every 1000ms
        t1 = QTimer(self); t1.timeout.connect(self.update); t1.start(1000)

        # Graph: watch for file changes via QFileSystemWatcher (no polling)
        self._graph_watcher = QFileSystemWatcher([GRAPH_PNG, GRAPH_JSON], self)
        self._graph_watcher.fileChanged.connect(self._on_graph_changed)

        self._edit_btn = QPushButton("✚ EDIT GRAPH", self)
        self._edit_btn.setCursor(Qt.PointingHandCursor)
        self._edit_btn.setStyleSheet("""
            QPushButton {
                color: #98c379; background: rgba(62, 68, 81, 160);
                border: 1px solid #3e4451; border-radius: 3px;
                font: bold 9pt 'JetBrains Mono'; padding: 3px 10px;
            }
            QPushButton:hover { background: rgba(97, 175, 239, 200); border-color: #61afef; }
        """)
        self._edit_btn.clicked.connect(self._edit_graph)

    def _edit_graph(self):
        script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "graph-edit.sh")
        subprocess.Popen([script], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def resizeEvent(self, e):
        self._edit_btn.adjustSize()
        self._edit_btn.move(self.width() - self._edit_btn.width() - 16, 100)
        super().resizeEvent(e)

    def _load_graph(self):
        try:
            self._px = QPixmap(GRAPH_PNG)
            self._mtime = os.path.getmtime(GRAPH_PNG)
        except Exception:
            self._px = None
        try:
            d = json.load(open(GRAPH_JSON))
            self._nodes = len(d.get("nodes", []))
            self._edges = len(d.get("edges", []))
        except Exception:
            pass

    def _on_graph_changed(self, path):
        """Called by QFileSystemWatcher when graph.png or graph.json changes."""
        self._load_graph()
        self.update()


    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        # no fill — shared bluish background comes from the deck window

        # ── Bottom layer: animated pipes texture ────────────────────
        self._pipes.paint(p)

        # ── Clock ─────────────────────────────────────────────────
        now = datetime.now()
        clock_f = QFont("JetBrains Mono", 34, QFont.Bold)
        p.setFont(clock_f); p.setPen(CYAN)
        ts = now.strftime("%H:%M:%S")
        tw = QFontMetrics(clock_f).horizontalAdvance(ts)
        p.drawText((w - tw) // 2, 58, ts)

        date_f = QFont("JetBrains Mono", 11)
        p.setFont(date_f); p.setPen(GREEN)
        ds = now.strftime("%a  %d %b %Y")
        dw = QFontMetrics(date_f).horizontalAdvance(ds)
        p.drawText((w - dw) // 2, 80, ds)

        p.setPen(QPen(DIM, 1)); p.drawLine(16, 95, w - 16, 95)

        lbl_f = QFont("JetBrains Mono", 10, QFont.Bold)
        p.setFont(lbl_f); p.setPen(GREEN)
        p.drawText(16, 114, "▶  MISSION GRAPH")

        # ── Graph image — full width now, no crop/seam ──────────────
        graph_top = 120
        graph_h   = h - graph_top - 40   # leave room for telemetry strip
        if self._px and not self._px.isNull():
            src = self._px.rect()
            p.drawPixmap(QRect(0, graph_top, w, graph_h), self._px, src)
        else:
            p.setPen(DIM)
            p.setFont(QFont("JetBrains Mono", 11))
            p.drawText(QRect(0, graph_top, w, graph_h), Qt.AlignCenter, "[ no graph ]")

        # ── Telemetry strip — one line, spans the full merged width ──
        sy = h - 34
        p.setPen(QPen(DIM, 1)); p.drawLine(16, sy, w - 16, sy)
        p.setFont(QFont("JetBrains Mono", 9))

        try:
            secs = float(open("/proc/uptime").read().split()[0])
            upt  = f"{int(secs // 3600)}h {int((secs % 3600) // 60)}m"
        except Exception:
            upt = "?"
        try:
            kern = os.uname().release.split("-")[0]
        except Exception:
            kern = "?"

        tx = 16
        p.setPen(GREEN)
        s = f"◈ Nodes: {self._nodes}   Links: {self._edges}"
        p.drawText(tx, sy + 16, s)
        tx += QFontMetrics(p.font()).horizontalAdvance(s) + 30

        p.setPen(WHITE)
        s = f"◈ Uptime: {upt}"
        p.drawText(tx, sy + 16, s)
        tx += QFontMetrics(p.font()).horizontalAdvance(s) + 30

        p.setPen(CYAN)
        p.drawText(tx, sy + 16, f"◈ Kernel: {kern}")

        p.end()