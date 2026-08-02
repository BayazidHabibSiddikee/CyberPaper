#!/usr/bin/env python3
"""
cyberdeck.py — Single transparent PySide6 window that sits as the desktop
background. Three columns (left: animations, center: graph+clock,
right: system shelf) + a bottom bar. Everything in ONE window — no
floating conky panels, no xwinwrap fights with i3.

Usage:  python3 cyberdeck.py [--screen WxH]
"""
import sys, os, signal, argparse, Xlib, Xlib.display, Xlib.Xatom

# Allow running from any directory
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from PySide6.QtWidgets import QApplication, QWidget, QHBoxLayout, QVBoxLayout
from PySide6.QtCore    import Qt, QTimer
from PySide6.QtGui     import QPainter, QColor

from main_panel   import MainPanel
from right_panel  import RightPanel
from bottom_bar   import BottomBar
from spectrum_overlay import SpectrumOverlay


def setup_x11_below(wid):
    """Set window as _NET_WM_STATE_BELOW — above desktop layer, below apps."""
    d = Xlib.display.Display()
    w = d.create_resource_object('window', wid)
    w.change_property(
        d.intern_atom('_NET_WM_STATE'),
        Xlib.Xatom.ATOM, 32,
        [
            d.intern_atom('_NET_WM_STATE_BELOW'),
            d.intern_atom('_NET_WM_STATE_SKIP_TASKBAR'),
            d.intern_atom('_NET_WM_STATE_SKIP_PAGER'),
        ]
    )
    d.sync()


# Column width percentages
LEFT_PCT   = 28
CENTER_PCT = 44   # 100 - 28 - 28
RIGHT_PCT  = 28
BOTTOM_H   = 32   # px

# CYBERDECK_GLAVA=1 turns on the built-in, transparent bottom-up audio
# spectrum overlay across the graph area (left + center columns) — see
# spectrum_overlay.py. Off by default so the deck stays lightweight when
# you don't want it.
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

        # Keep the window at the bottom of the X stacking order.
        # Override-redirect windows aren't managed by the WM, so we must
        # periodically lower ourselves to stay below all normal windows.
        self._lower_timer = QTimer(self)
        self._lower_timer.timeout.connect(self._lower_below)
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

        # Left + center are now a single merged panel — one pixmap, one
        # paintEvent, no crop_x seam between two separately-drawn widgets.
        self._main  = MainPanel(cols_widget)
        self._right = RightPanel(cols_widget)

        self._main .setFixedSize(lw + cw, top_h)
        self._right.setFixedSize(rw, top_h)

        cols.addWidget(self._main)
        cols.addWidget(self._right)

        # ── Bottom-up audio spectrum, overlaid on the graph area ────────
        # Sits above the left+center columns (same combined width/height
        # as the graph they share), transparent and click-through, so the
        # graph and pipes texture stay visible underneath it.
        self._spectrum = None
        if GLAVA_MODE:
            self._spectrum = SpectrumOverlay(self)
            self._spectrum.setGeometry(0, 0, lw + cw, top_h)

        # Bottom bar: a separate top-level DOCK window. i3 manages docks
        # specially — always visible, screen space reserved (like i3bar).
        self._bottom = BottomBar()
        self._bottom.setWindowFlags(Qt.FramelessWindowHint)
        self._bottom.setAttribute(Qt.WA_X11NetWmWindowTypeDock)
        self._bottom.setAttribute(Qt.WA_ShowWithoutActivating)
        self._bottom.setFixedSize(sw, BOTTOM_H)
        self._bottom.move(0, sh - BOTTOM_H)
        self._bottom.setWindowTitle("sworddeck-bar")

        root.addWidget(cols_widget)
        root.addStretch(1)

        if self._spectrum is not None:
            self._spectrum.raise_()

        # ── Graph render triggered only by graph-edit.sh (no polling) ──

    def _lower_below(self):
        """Ensure correct stacking: glava (DESKTOP) < cyberdeck (BELOW) < apps.

        Only lowers this window. glava's DESKTOP/BELOW properties are set once at
        startup by cyberdesk.sh; X11 properties are durable, so re-applying them
        every tick spawned ~259k xdotool/xprop processes a day and changed nothing.
        """
        self.lower()

    def paintEvent(self, _):
        # Solid dark fill behind everything (panels + spectrum overlay sit
        # on top of this same flat background).
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(40, 44, 52, 255))
        p.end()

    def keyPressEvent(self, e):
        # Ctrl+C / Escape quit
        if e.key() in (Qt.Key_Escape,) and e.modifiers() & Qt.ControlModifier:
            QApplication.quit()

    def showEvent(self, e):
        super().showEvent(e)
        QTimer.singleShot(300, self._apply_x11)

    def _apply_x11(self):
        wid = int(self.winId())
        try:
            setup_x11_below(wid)
            print(f"[cyberdeck] X11 BELOW hints applied (window {wid})")
        except Exception as e:
            print(f"[cyberdeck] X11 error: {e}")


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
    win._bottom.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
