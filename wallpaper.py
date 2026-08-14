#!/usr/bin/env python3
"""
Animated Wallpaper for i3 — proper X11 desktop background.
Uses python-xlib to set _NET_WM_WINDOW_TYPE_DESKTOP BEFORE showing.
Keybinds, rofi, $mod+Q all work normally.
"""

import sys, math, random, time, struct, subprocess, os, signal
from pathlib import Path

os.environ["QT_QPA_PLATFORM"] = "xcb"

from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout
from PySide6.QtCore import Qt, QTimer, QPoint, Signal, QThread
from PySide6.QtGui import QPainter, QColor, QLinearGradient, QRadialGradient, QPen, QFont, QPainterPath

import Xlib
import Xlib.display
import Xlib.Xatom


def clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


# ── X11: Set window as desktop BEFORE showing ────────────────────────────────

def setup_x11(wid):
    """Set window as _NET_WM_WINDOW_TYPE_DESKTOP — below all, no input."""
    d = Xlib.display.Display()
    w = d.create_resource_object('window', wid)

    # Window type = DESKTOP
    w.change_property(
        d.intern_atom('_NET_WM_WINDOW_TYPE'),
        Xlib.Xatom.ATOM, 32,
        [d.intern_atom('_NET_WM_WINDOW_TYPE_DESKTOP')]
    )

    # State = BELOW + SKIP_TASKBAR + SKIP_PAGER
    w.change_property(
        d.intern_atom('_NET_WM_STATE'),
        Xlib.Xatom.ATOM, 32,
        [
            d.intern_atom('_NET_WM_STATE_BELOW'),
            d.intern_atom('_NET_WM_STATE_SKIP_TASKBAR'),
            d.intern_atom('_NET_WM_STATE_SKIP_PAGER'),
        ]
    )

    # Sticky on all desktops
    w.change_property(
        d.intern_atom('_NET_WM_DESKTOP'),
        Xlib.Xatom.CARDINAL, 32,
        [0xFFFFFFFF]
    )

    # Set strutReserved to avoid panel overlap
    # Not critical but nice to have

    d.sync()


# ── Audio Thread ──────────────────────────────────────────────────────────────

class AudioThread(QThread):
    levels = Signal(list)

    def __init__(self, n=32):
        super().__init__()
        self.n = n
        self.data = [0.0] * n
        self._run = True

    def run(self):
        try:
            proc = subprocess.Popen(
                ["parec", "--format=float32le", "--rate=44100", "--channels=1"],
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            )
            while self._run:
                raw = proc.stdout.read(self.n * 4)
                if not raw:
                    break
                vals = struct.unpack(f"{self.n}f", raw)
                for i, v in enumerate(vals):
                    self.data[i] = self.data[i] * 0.6 + min(abs(v) * 4.0, 1.0) * 0.4
                self.levels.emit(self.data[:])
        except FileNotFoundError:
            while self._run:
                t = time.time()
                for i in range(self.n):
                    v = math.sin(t * 2 + i * 0.3) * 0.3 + 0.3
                    self.data[i] = self.data[i] * 0.7 + clamp(v) * 0.3
                self.levels.emit(self.data[:])
                time.sleep(0.04)

    def stop(self):
        self._run = False
        self.wait(1000)


# ── Renderers ─────────────────────────────────────────────────────────────────

class GradientRenderer(QWidget):
    def __init__(self):
        super().__init__()
        self.hue = random.random()
        self._t = QTimer(self, timeout=self.tick)
        self._t.start(50)

    def tick(self):
        self.hue = (self.hue + 0.001) % 1.0
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        w, h = self.width(), self.height()
        t = time.time()
        h1 = int(self.hue * 359)
        c1 = QColor.fromHsv(h1, 40, 30)
        c2 = QColor.fromHsv((h1 + 140) % 360, 50, 18)
        g = QRadialGradient(w * 0.5, h * 0.5, max(w, h) * 0.7)
        g.setColorAt(0, c1)
        g.setColorAt(1, c2)
        p.fillRect(0, 0, w, h, g)
        for i in range(6):
            ox = w * (0.15 + 0.7 * math.sin(t * 0.12 + i * 1.1))
            oy = h * (0.2 + 0.6 * math.cos(t * 0.09 + i * 0.8))
            r = 100 + 80 * math.sin(t * 0.2 + i)
            orb = QRadialGradient(ox, oy, max(r, 1))
            h2 = int((self.hue + i * 0.12) * 359) % 360
            orb.setColorAt(0, QColor.fromHsv(h2, 30, 14, 35))
            orb.setColorAt(1, QColor(0, 0, 0, 0))
            p.fillRect(0, 0, w, h, orb)
        p.end()


class ParticleRenderer(QWidget):
    def __init__(self):
        super().__init__()
        self.p = [{'x': random.random(), 'y': random.random(),
                    'vx': random.uniform(-0.3, 0.3) / 1000,
                    'vy': random.uniform(-0.1, 0.2) / 1000,
                    'sz': random.uniform(1, 3), 'a': random.uniform(50, 200),
                    'h': random.randint(0, 359), 'p': random.uniform(0.5, 2)}
                   for _ in range(120)]
        self._t = QTimer(self, timeout=self.tick)
        self._t.start(50)

    def tick(self):
        for pt in self.p:
            pt['x'] = (pt['x'] + pt['vx']) % 1.0
            pt['y'] = (pt['y'] + pt['vy']) % 1.0
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        w, h = self.width(), self.height()
        p.fillRect(0, 0, w, h, QColor(10, 10, 18))
        t = time.time()
        p.setPen(Qt.NoPen)
        for pt in self.p:
            sx, sy = int(pt['x'] * w), int(pt['y'] * h)
            pulse = 0.5 + 0.5 * math.sin(t * pt['p'])
            a = int(pt['a'] * pulse)
            r = pt['sz'] * (0.7 + 0.3 * pulse)
            p.setBrush(QColor.fromHsv(pt['h'], 40, 230, a))
            p.drawEllipse(sx - int(r), sy - int(r), int(r * 2), int(r * 2))
            g = r * 3
            p.setBrush(QColor.fromHsv(pt['h'], 25, 255, max(1, int(a * 0.1))))
            p.drawEllipse(sx - int(g), sy - int(g), int(g * 2), int(g * 2))
        p.end()


class WaveRenderer(QWidget):
    def __init__(self):
        super().__init__()
        self._t = QTimer(self, timeout=self.update)
        self._t.start(50)

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        p.fillRect(0, 0, w, h, QColor(8, 8, 16))
        t = time.time()
        layers = [
            (0.55, 40, 0.008, 0.7, 0.55, 80),
            (0.65, 30, 0.012, 0.5, 0.60, 60),
            (0.75, 25, 0.015, 1.0, 0.50, 50),
            (0.45, 50, 0.006, 0.3, 0.65, 40),
        ]
        for hue_f, amp, freq, spd, yoff, alpha in layers:
            pts = []
            for x in range(0, w + 4, 4):
                y = h * yoff + amp * math.sin(x * freq + t * spd)
                y += amp * 0.5 * math.sin(x * freq * 2.3 + t * spd * 1.4)
                pts.append(QPoint(x, int(clamp(y, 0, h))))
            qp = QPainterPath()
            qp.moveTo(pts[0])
            for pt in pts[1:]:
                qp.lineTo(pt)
            qp.lineTo(w, h)
            qp.lineTo(0, h)
            qp.closeSubpath()
            hi = int(hue_f * 359) % 360
            p.setPen(Qt.NoPen)
            p.setBrush(QColor.fromHsv(hi, 76, 38, alpha))
            p.drawPath(qp)
            lp = QPainterPath()
            lp.moveTo(pts[0])
            for pt in pts[1:]:
                lp.lineTo(pt)
            p.setPen(QPen(QColor.fromHsv(hi, 102, 153, alpha + 40), 2))
            p.setBrush(Qt.NoBrush)
            p.drawPath(lp)
        p.end()


class MatrixRenderer(QWidget):
    def __init__(self):
        super().__init__()
        self.cols = []
        self._t = QTimer(self, timeout=self.tick)
        self._t.start(55)
        self._init_cols()

    def _init_cols(self):
        w = max(self.width(), 800)
        self.cols = [{'x': i * 14, 'y': random.randint(-600, 0),
                       'sp': random.randint(3, 9),
                       'ch': [chr(random.randint(0x30A0, 0x30FF)) for _ in range(30)],
                       'ln': random.randint(8, 22),
                       'br': random.randint(76, 255)}
                      for i in range(w // 14)]

    def tick(self):
        h = self.height()
        for c in self.cols:
            c['y'] += c['sp']
            if c['y'] > h + 500:
                c['y'] = random.randint(-400, -50)
                c['sp'] = random.randint(3, 9)
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        w, h = self.width(), self.height()
        p.fillRect(0, 0, w, h, QColor(0, 0, 0))
        p.setFont(QFont("monospace", 12))
        for c in self.cols:
            for j in range(c['ln']):
                cy = c['y'] + j * 16
                if cy < -20 or cy > h + 20:
                    continue
                if j == 0:
                    col = QColor(180, 255, 180, 255)
                elif j < 3:
                    f = 1.0 - j * 0.25
                    col = QColor(0, int(255 * f), 0, int(255 * f))
                else:
                    f = max(0.0, 1.0 - j / c['ln'])
                    col = QColor(0, int(200 * f), 0, int(f * c['br']))
                p.setPen(col)
                ch = c['ch'][j % len(c['ch'])]
                if random.random() < 0.02:
                    c['ch'][j % len(c['ch'])] = chr(random.randint(0x30A0, 0x30FF))
                    ch = c['ch'][j % len(c['ch'])]
                p.drawText(c['x'], cy, ch)
        p.end()


class GraphRenderer(QWidget):
    def __init__(self):
        super().__init__()
        self.n = 48
        self.raw = [0.0] * self.n
        self.smooth = [0.0] * self.n
        self._audio = AudioThread(self.n)
        self._audio.levels.connect(self._on_audio)
        self._audio.start()
        self._t = QTimer(self, timeout=self.tick)
        self._t.start(33)

    def _on_audio(self, data):
        self.raw = data

    def tick(self):
        for i in range(self.n):
            r = self.raw[i] if i < len(self.raw) else 0.0
            self.smooth[i] = self.smooth[i] * 0.75 + r * 0.25
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        p.fillRect(0, 0, w, h, QColor(8, 8, 14))
        bw = max(4, (w - 80) // self.n - 2)
        tw = self.n * (bw + 2)
        sx = (w - tw) // 2
        for i in range(self.n):
            bh = int(self.smooth[i] * h * 0.6)
            x = sx + i * (bw + 2)
            y = h - bh - 40
            hi = (198 + i * 3) % 360
            g = QLinearGradient(x, y + bh, x, y)
            g.setColorAt(0, QColor.fromHsv(hi, 153, 38, 200))
            g.setColorAt(0.5, QColor.fromHsv(hi, 127, 102, 220))
            g.setColorAt(1, QColor.fromHsv(hi, 76, 204, 240))
            p.setPen(Qt.NoPen)
            p.setBrush(g)
            p.drawRoundedRect(x, y, bw, bh, 3, 3)
            p.setBrush(QColor.fromHsv(hi, 102, 25, 60))
            p.drawRoundedRect(x, h - 38, bw, max(1, bh // 5), 2, 2)
            p.setBrush(QColor.fromHsv(hi, 51, 255, 100))
            p.drawEllipse(x + bw // 2 - 4, y - 4, 8, 8)
        p.setPen(QPen(QColor(255, 255, 255, 8), 1))
        for yl in range(0, h, 4):
            p.drawLine(0, yl, w, yl)
        p.end()

    def closeEvent(self, e):
        self._audio.stop()
        self._audio.deleteLater()


# ── Modes ─────────────────────────────────────────────────────────────────────

MODES = ['gradient', 'particles', 'wave', 'matrix', 'graph']
RENDERERS = {
    'gradient': GradientRenderer,
    'particles': ParticleRenderer,
    'wave': WaveRenderer,
    'matrix': MatrixRenderer,
    'graph': GraphRenderer,
}


# ── State ─────────────────────────────────────────────────────────────────────

STATE = Path.home() / ".config" / "animated-wallpaper" / "state"
PID = Path.home() / ".config" / "animated-wallpaper" / "pid"


def get_state():
    return STATE.read_text().strip() if STATE.exists() else "gradient"


def set_state(m):
    STATE.parent.mkdir(parents=True, exist_ok=True)
    STATE.write_text(m)


def next_mode():
    i = MODES.index(get_state()) if get_state() in MODES else 0
    n = MODES[(i + 1) % len(MODES)]
    set_state(n)
    return n


# ── Main Window ───────────────────────────────────────────────────────────────

class Wallpaper(QWidget):
    def __init__(self, mode):
        super().__init__()
        self.mode = mode
        self.renderer = None
        self._layout = QVBoxLayout(self)
        self._layout.setContentsMargins(0, 0, 0, 0)

        # Frameless + below everything
        # WindowTransparentForInput = clicks/keys pass through
        self.setWindowFlags(
            Qt.FramelessWindowHint |
            Qt.WindowStaysOnBottomHint |
            Qt.Tool |
            Qt.WindowTransparentForInput
        )
        self.setAttribute(Qt.WA_ShowWithoutActivating)
        self.setAttribute(Qt.WA_TranslucentBackground, False)
        self.setFocusPolicy(Qt.NoFocus)

        screen = QApplication.primaryScreen().geometry()
        self.setGeometry(0, 0, screen.width(), screen.height())

        self._set_renderer(mode)

    def _set_renderer(self, mode):
        if self.renderer:
            self._layout.removeWidget(self.renderer)
            self.renderer.deleteLater()
        cls = RENDERERS.get(mode, GradientRenderer)
        self.renderer = cls()
        self._layout.addWidget(self.renderer)
        self.mode = mode
        set_state(mode)

    def switch(self, mode):
        if mode in RENDERERS:
            self._set_renderer(mode)

    def showEvent(self, e):
        super().showEvent(e)
        QTimer.singleShot(300, self._apply_x11)

    def _apply_x11(self):
        wid = int(self.winId())
        try:
            setup_x11(wid)
            print(f"[wallpaper] X11 desktop hints applied (window {wid})")
        except Exception as e:
            print(f"[wallpaper] X11 error: {e}")


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=MODES, default=None)
    ap.add_argument("--next", action="store_true")
    ap.add_argument("--stop", action="store_true")
    ap.add_argument("--list", action="store_true")
    args = ap.parse_args()

    if args.list:
        print("Modes:", ", ".join(MODES))
        return

    if args.stop:
        if PID.exists():
            try:
                os.kill(int(PID.read_text().strip()), signal.SIGTERM)
            except Exception:
                pass
            PID.unlink(missing_ok=True)
        print("Stopped.")
        return

    # Kill old
    if PID.exists():
        try:
            os.kill(int(PID.read_text().strip()), signal.SIGTERM)
            time.sleep(0.15)
        except Exception:
            pass

    mode = next_mode() if args.next else (args.mode or get_state())
    set_state(mode)

    app = QApplication(sys.argv)
    app.setApplicationName("AnimatedWallpaper")

    w = Wallpaper(mode)
    w.show()
    PID.parent.mkdir(parents=True, exist_ok=True)
    PID.write_text(str(os.getpid()))

    print(f"[wallpaper] Mode: {mode}")
    print("[wallpaper] Desktop background — rofi/$mod+Q work normally")

    app.aboutToQuit.connect(lambda: PID.unlink(missing_ok=True))
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
