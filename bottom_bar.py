"""bottom_bar.py — single-line stat bar"""
import os, subprocess
from datetime import datetime
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QPainter, QColor, QFont, QFontMetrics

CYAN  = QColor(144, 220, 255)
GREEN = QColor(150, 240, 200)
AMBER = QColor(255, 200, 0)
RED   = QColor(255, 70, 70)
DIM   = QColor(42, 90, 122)
BG    = QColor(0, 8, 17, 210)


def _cpu():
    try:
        vals = list(map(int, open("/proc/stat").readline().split()[1:8]))
        idle, total = vals[3], sum(vals)
        if not hasattr(_cpu, "_p"):
            _cpu._p = (total, idle); return 0
        pt, pi = _cpu._p; _cpu._p = (total, idle)
        dt, di = total - pt, idle - pi
        return int(100 * (1 - di / dt)) if dt else 0
    except Exception:
        return 0


def _mem_pct():
    try:
        info = {}
        for line in open("/proc/meminfo"):
            k, v = line.split(":", 1)
            info[k.strip()] = int(v.split()[0])
        return int(100 * (info["MemTotal"] - info["MemAvailable"]) / info["MemTotal"])
    except Exception:
        return 0


def _disk_pct():
    try:
        st = os.statvfs("/")
        return int(100 * (1 - st.f_bavail / st.f_blocks))
    except Exception:
        return 0


def _net():
    try:
        script = os.path.expanduser("~/animated-wallpaper/net-speed.sh")
        if os.path.exists(script):
            out = subprocess.check_output([script], timeout=1, text=True).strip()
            return out
        return "? KB/s"
    except Exception:
        return "? KB/s"


def _music():
    try:
        out = subprocess.check_output(
            ["playerctl", "metadata", "--format", "♫ {{artist}} – {{title}}"],
            timeout=1, text=True, stderr=subprocess.DEVNULL
        ).strip()
        return (out[:55] + "…") if len(out) > 55 else out
    except Exception:
        return "♫ —"


def _uptime():
    try:
        secs = float(open("/proc/uptime").read().split()[0])
        return f"↑ {int(secs//3600)}h {int((secs%3600)//60)}m"
    except Exception:
        return "↑ ?"


class BottomBar(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self._stats = {}
        t = QTimer(self); t.timeout.connect(self._refresh); t.start(2000)
        QTimer.singleShot(100, self._refresh)

    def _refresh(self):
        self._stats = {
            "cpu": _cpu(), "mem": _mem_pct(),
            "disk": _disk_pct(), "net": _net(),
            "music": _music(), "uptime": _uptime(),
        }
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        W, H = self.width(), self.height()
        p.fillRect(self.rect(), BG)

        f = QFont("JetBrains Mono", 10, QFont.Bold)
        p.setFont(f)
        fm = QFontMetrics(f)

        now = datetime.now()
        s = self._stats
        cpu = s.get("cpu", 0); mem = s.get("mem", 0); disk = s.get("disk", 0)

        def cpu_color(v):
            return RED if v >= 80 else AMBER if v >= 50 else GREEN

        SEP = "  │  "
        parts = [
            (CYAN,         f"[CYBER]  {now.strftime('%H:%M:%S  %a %d %b')}"),
            (DIM,          SEP),
            (GREEN,        "⚡ CPU: "),
            (cpu_color(cpu), f"{cpu}%"),
            (DIM,          SEP),
            (GREEN,        "🧠 RAM: "),
            (CYAN,         f"{mem}%"),
            (DIM,          SEP),
            (GREEN,        "💾 /: "),
            (CYAN,         f"{disk}%"),
            (DIM,          SEP),
            (CYAN,         s.get("net", "?")),
            (DIM,          SEP),
            (GREEN,        s.get("music", "♫ —")),
            (DIM,          SEP),
            (CYAN,         s.get("uptime", "?")),
        ]

        x = 10
        y = H - (H - fm.ascent()) // 2 - 2
        for color, text in parts:
            p.setPen(color)
            p.drawText(x, y, text)
            x += fm.horizontalAdvance(text)

        p.end()
