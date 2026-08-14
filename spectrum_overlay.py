"""spectrum_overlay.py — a built-in, transparent bottom-up audio spectrum
that overlays the mission-graph area (the left + center columns).

This replaces relying on an external `glava` window: it's drawn directly
inside the deck as the top-most layer over that region, click-through, and
kept low-alpha enough that the mission graph and the pipes texture stay
clearly visible underneath the bars.

Enabled with CYBERDECK_GLAVA=1 (see cyberdeck.py).
"""
import math, struct, subprocess, time
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import Qt, QTimer, Signal, QThread
from PySide6.QtGui import QPainter, QColor, QLinearGradient

# Cyan, matching the previous `glava --desktop -m graph` look — a single
# hue rather than the hue-cycling rainbow, so it reads as one coherent
# glow rising up from the baseline instead of a multicolor equalizer.
CYAN_LOW  = QColor(30, 60, 100)     # One Dark blue dim
CYAN_HIGH = QColor(97, 175, 239)    # One Dark blue


def clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


class AudioThread(QThread):
    """Reads live audio levels via PulseAudio's `parec`. Falls back to a
    gentle idle wave if parec/pulseaudio isn't available, so the overlay
    never just crashes or sits frozen."""
    levels = Signal(list)

    def __init__(self, n=48):
        super().__init__()
        self.n = n
        self.data = [0.0] * n
        self._run = True

    def run(self):
        try:
            # Record what's PLAYING (the default sink's monitor), not the
            # microphone — parec with no -d captures the default source.
            monitor = None
            try:
                sink = subprocess.check_output(
                    ["pactl", "get-default-sink"], timeout=2, text=True).strip()
                if sink:
                    monitor = sink + ".monitor"
            except Exception:
                pass
            cmd = ["parec", "--format=float32le", "--rate=44100", "--channels=1"]
            if monitor:
                cmd += ["-d", monitor]
            proc = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
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
                    v = math.sin(t * 1.4 + i * 0.25) * 0.15 + 0.15
                    self.data[i] = self.data[i] * 0.7 + clamp(v) * 0.3
                self.levels.emit(self.data[:])
                time.sleep(0.05)

    def stop(self):
        self._run = False
        self.wait(1000)


class SpectrumOverlay(QWidget):
    """Transparent, click-through equalizer bars anchored to the bottom of
    the region it's given, rising upward with the music."""

    def __init__(self, parent=None, n=48):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)
        self.n = n
        self.raw = [0.0] * n
        self.smooth = [0.0] * n

        self._audio = AudioThread(n)
        self._audio.levels.connect(self._on_audio)
        self._audio.start()

        self._t = QTimer(self)
        self._t.timeout.connect(self._tick)
        self._t.start(33)

    def _on_audio(self, data):
        self.raw = data

    def _tick(self):
        for i in range(self.n):
            r = self.raw[i] if i < len(self.raw) else 0.0
            self.smooth[i] = self.smooth[i] * 0.78 + r * 0.22
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        bw = max(4, w // self.n - 2)
        tw = self.n * (bw + 2)
        sx = (w - tw) // 2
        for i in range(self.n):
            bh = int(self.smooth[i] * h * 0.55)
            if bh <= 0:
                continue
            x = sx + i * (bw + 2)
            y = h - bh
            # Low alpha throughout — this rides on top of the graph and
            # pipes texture, so it needs to stay see-through rather than
            # painting a solid wall of bars over them. Single cyan hue,
            # dim at the base and brighter toward the tip.
            g = QLinearGradient(x, y + bh, x, y)
            g.setColorAt(0.0, QColor(CYAN_LOW.red(), CYAN_LOW.green(), CYAN_LOW.blue(), 35))
            g.setColorAt(1.0, QColor(CYAN_HIGH.red(), CYAN_HIGH.green(), CYAN_HIGH.blue(), 130))
            p.setPen(Qt.NoPen)
            p.setBrush(g)
            p.drawRoundedRect(x, y, bw, bh, 2, 2)
        p.end()

    def closeEvent(self, e):
        self._audio.stop()
        self._audio.deleteLater()
