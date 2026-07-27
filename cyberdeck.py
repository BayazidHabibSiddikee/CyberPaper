#!/usr/bin/env python3
"""
cyberdeck.py — Single transparent PySide6 window that sits as the desktop
background. Three columns (left: animations, center: graph+clock,
right: system shelf) + a bottom bar. Everything in ONE window — no
floating conky panels, no xwinwrap fights with i3.

Usage:  python3 cyberdeck.py [--screen WxH]
"""
import sys, os, signal, subprocess, argparse

# Allow running from any directory
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from PySide6.QtWidgets import QApplication, QWidget, QHBoxLayout, QVBoxLayout
from PySide6.QtCore    import Qt, QTimer
from PySide6.QtGui     import QPainter, QColor

from left_panel   import LeftPanel
from center_panel import CenterPanel
from right_panel  import RightPanel
from bottom_bar   import BottomBar

# Column width percentages
LEFT_PCT   = 28
CENTER_PCT = 44   # 100 - 28 - 28
RIGHT_PCT  = 28
BOTTOM_H   = 32   # px

# When glava runs below us as the audio visualizer, the left column must be
# a transparent hole so the glava window shows through.
GLAVA_MODE = os.environ.get("CYBERDECK_GLAVA") == "1"


class CyberDeck(QWidget):
    """
    Single full-screen window on the desktop layer.
    i3 / the WM sees it as a desktop window and keeps it behind everything.
    Your normal work windows sit on top — you see the cyberdeck as the
    background when no window covers that zone.
    """
    def __init__(self, sw: int, sh: int):
        super().__init__()
        self.SW, self.SH = sw, sh

        # ── Window flags ──────────────────────────────────────────
        # BypassWindowManagerHint  → X11 override_redirect; WM won't manage it
        # Tool                     → no taskbar entry
        # BypassWindowManagerHint → override-redirect: i3 never manages/tiles
        # this window. We keep it lowered so it acts as the wallpaper layer.
        self.setWindowFlags(
            Qt.FramelessWindowHint |
            Qt.BypassWindowManagerHint |
            Qt.Tool
        )
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAttribute(Qt.WA_ShowWithoutActivating)
        self.setGeometry(0, 0, sw, sh)
        self.setWindowTitle("cyberdeck")

        # Keep the window at the bottom of the X stacking order (new windows
        # always map above, so re-lower periodically; glava lowers itself too)
        self._lower_timer = QTimer(self)
        self._lower_timer.timeout.connect(self.lower)
        self._lower_timer.start(1000)

        # ── Layout ────────────────────────────────────────────────
        root = QVBoxLayout(self)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # Top strip: 3 columns
        cols_widget = QWidget(self)
        cols_widget.setAttribute(Qt.WA_TranslucentBackground)
        cols = QHBoxLayout(cols_widget)
        cols.setContentsMargins(0, 0, 0, 0)
        cols.setSpacing(0)

        lw = sw * LEFT_PCT   // 100
        rw = sw * RIGHT_PCT  // 100
        cw = sw - lw - rw

        top_h = sh - BOTTOM_H

        self._left   = LeftPanel(cols_widget)
        self._center = CenterPanel(cols_widget)
        self._right  = RightPanel(cols_widget)

        self._left  .setFixedSize(lw, top_h)
        self._center.setFixedSize(cw, top_h)
        self._right .setFixedSize(rw, top_h)

        cols.addWidget(self._left)
        cols.addWidget(self._center)
        cols.addWidget(self._right)

        # Bottom bar
        self._bottom = BottomBar(self)
        self._bottom.setFixedSize(sw, BOTTOM_H)

        root.addWidget(cols_widget)
        root.addWidget(self._bottom)

        # ── Also render a dark graph background render trigger ────
        # Re-render graph PNG every 10s if graph.json changed
        self._graph_timer = QTimer(self)
        self._graph_timer.timeout.connect(self._maybe_render_graph)
        self._graph_timer.start(10_000)
        self._graph_mtime = 0

    def _maybe_render_graph(self):
        cfg = os.path.expanduser("~/.config/animated-wallpaper")
        jf  = os.path.join(cfg, "graph.json")
        try:
            mt = os.path.getmtime(jf)
            if mt != self._graph_mtime:
                self._graph_mtime = mt
                script = os.path.join(HERE, "graph-render.sh")
                cw = self.SW * CENTER_PCT // 100
                ch = self.SH - BOTTOM_H
                subprocess.Popen(
                    [script, str(cw), str(ch)],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
                )
        except Exception:
            pass

    def paintEvent(self, _):
        # Solid dark fill behind panels (entire window background)
        p = QPainter(self)
        # glava (transparent, stacked just above us at the bottom of the
        # stack) draws its spectrum over the left column's dark fill.
        p.fillRect(self.rect(), QColor(2, 6, 18, 255))
        p.end()

    def keyPressEvent(self, e):
        # Ctrl+C / Escape quit
        if e.key() in (Qt.Key_Escape,) and e.modifiers() & Qt.ControlModifier:
            QApplication.quit()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--screen", default="1920x1080",
                    help="WxH e.g. 1920x1080")
    args = ap.parse_args()
    try:
        sw, sh = map(int, args.screen.split("x"))
    except Exception:
        sw, sh = 1920, 1080

    # Handle SIGTERM cleanly (so cyberdesk.sh stop works)
    signal.signal(signal.SIGTERM, lambda *_: QApplication.quit())

    app = QApplication(sys.argv)
    app.setApplicationName("cyberdeck")

    # Auto-detect screen size if not overridden
    screen = app.primaryScreen()
    if args.screen == "1920x1080":
        sw = screen.size().width()
        sh = screen.size().height()

    win = CyberDeck(sw, sh)
    win.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
