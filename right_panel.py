"""right_panel.py — system stats shelf"""
import os, subprocess
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import Qt, QTimer, QRect
from PySide6.QtGui import QPainter, QColor, QFont, QPen, QFontMetrics

CYAN   = QColor(0, 212, 255)
GREEN  = QColor(0, 255, 200)
AMBER  = QColor(255, 200, 0)
RED    = QColor(255, 70, 90)
DIM    = QColor(26, 58, 90)
WHITE  = QColor(200, 220, 255)
BG     = QColor(2, 5, 16, 215)


def _read(path, default="0"):
    try:
        return open(path).read().strip()
    except Exception:
        return default


def _cpu_percent():
    try:
        lines = open("/proc/stat").readlines()
        vals  = list(map(int, lines[0].split()[1:8]))
        idle  = vals[3]
        total = sum(vals)
        if not hasattr(_cpu_percent, "_prev"):
            _cpu_percent._prev = (total, idle)
            return 0.0
        pt, pi = _cpu_percent._prev
        _cpu_percent._prev = (total, idle)
        dt = total - pt; di = idle - pi
        return 100.0 * (1 - di / dt) if dt else 0.0
    except Exception:
        return 0.0


def _mem():
    try:
        info = {}
        for line in open("/proc/meminfo"):
            k, v = line.split(":", 1)
            info[k.strip()] = int(v.split()[0])
        total = info["MemTotal"]
        avail = info["MemAvailable"]
        used  = total - avail
        return used // 1024, total // 1024   # MB
    except Exception:
        return 0, 1


def _net_speed():
    try:
        iface = None
        for line in open("/proc/net/dev"):
            parts = line.split()
            name = parts[0].rstrip(":")
            if name not in ("lo", "") and len(parts) > 9:
                rx, tx = int(parts[1]), int(parts[9])
                if not hasattr(_net_speed, "_prev"):
                    _net_speed._prev = {}
                prev = _net_speed._prev.get(name, (rx, tx))
                _net_speed._prev[name] = (rx, tx)
                drx = max(0, rx - prev[0])
                dtx = max(0, tx - prev[1])
                if rx > 0:
                    iface = name
                    return iface, drx // 1024, dtx // 1024   # KB/s
        return "?", 0, 0
    except Exception:
        return "?", 0, 0


def _top_procs(n=6):
    try:
        out = subprocess.check_output(
            ["ps", "aux", "--sort=-%cpu"],
            timeout=2, text=True
        ).splitlines()[1:n+1]
        procs = []
        for line in out:
            parts = line.split(None, 10)
            if len(parts) >= 11:
                procs.append((parts[10][:18], float(parts[2]), float(parts[3])))
        return procs
    except Exception:
        return []


def _cpu_temp():
    for hw in range(8):
        for t in range(1, 5):
            path = f"/sys/class/hwmon/hwmon{hw}/temp{t}_input"
            try:
                val = int(open(path).read()) // 1000
                if 20 < val < 120:
                    return val
            except Exception:
                pass
    return None


class RightPanel(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self._cpu   = 0.0
        self._mem_used = 0
        self._mem_total = 1
        self._iface = "?"
        self._rx = 0
        self._tx = 0
        self._temp = None
        self._procs = []

        t = QTimer(self)
        t.timeout.connect(self._update_stats)
        t.start(2000)
        self._update_stats()

    def _update_stats(self):
        self._cpu          = _cpu_percent()
        self._mem_used, self._mem_total = _mem()
        self._iface, self._rx, self._tx = _net_speed()
        self._temp         = _cpu_temp()
        self._procs        = _top_procs(6)
        self.update()

    # ── helpers ──────────────────────────────────────────────────
    def _bar(self, p, x, y, bw, bh, pct, color):
        p.setPen(QPen(DIM, 1))
        p.drawRect(x, y, bw, bh)
        fill = int(bw * min(pct, 100) / 100)
        p.fillRect(x + 1, y + 1, max(0, fill - 2), bh - 1, color)

    def _section(self, p, x, y, w, label):
        p.setPen(QPen(DIM, 1))
        p.drawLine(x, y, x + w - 8, y)
        f = QFont("JetBrains Mono", 9, QFont.Bold)
        p.setFont(f); p.setPen(GREEN)
        p.drawText(x, y - 2, f"── {label} ──")
        return y + 14

    def _val_color(self, pct):
        if pct >= 80: return RED
        if pct >= 50: return AMBER
        return GREEN

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        W, H = self.width(), self.height()
        p.fillRect(self.rect(), BG)

        x   = 12
        bw  = W - 24
        y   = 10
        sm  = QFont("JetBrains Mono", 9)
        smb = QFont("JetBrains Mono", 9, QFont.Bold)

        # ── Header ───────────────────────────────────────────────
        f = QFont("JetBrains Mono", 16, QFont.Bold)
        p.setFont(f); p.setPen(CYAN)
        hw = QFontMetrics(f).horizontalAdvance("▸ CYBERDECK")
        p.drawText((W - hw) // 2, y + 20, "▸ CYBERDECK")
        y += 34
        p.setPen(QPen(DIM, 1)); p.drawLine(x, y, x + bw, y); y += 10

        # ── CPU ──────────────────────────────────────────────────
        y = self._section(p, x, y, W, "SYSTEM")
        cpu_c = self._val_color(self._cpu)
        p.setFont(smb); p.setPen(WHITE)
        p.drawText(x, y + 12, "CPU")
        p.setFont(sm); p.setPen(cpu_c)
        p.drawText(x + 35, y + 12, f"{self._cpu:.0f}%")
        if self._temp:
            p.setPen(AMBER); p.drawText(x + 80, y + 12, f"  {self._temp}°C")
        self._bar(p, x, y + 15, bw, 7, self._cpu, cpu_c)
        y += 28

        mem_pct = 100 * self._mem_used / max(1, self._mem_total)
        mem_c   = self._val_color(mem_pct)
        p.setFont(smb); p.setPen(WHITE); p.drawText(x, y + 12, "RAM")
        p.setFont(sm);  p.setPen(mem_c)
        p.drawText(x + 35, y + 12,
                   f"{self._mem_used}M / {self._mem_total}M  {mem_pct:.0f}%")
        self._bar(p, x, y + 15, bw, 7, mem_pct, mem_c)
        y += 30

        # ── Disk ─────────────────────────────────────────────────
        try:
            st = os.statvfs("/")
            disk_pct = 100 * (1 - st.f_bavail / st.f_blocks)
            disk_used = (st.f_blocks - st.f_bavail) * st.f_frsize // (1024**3)
            disk_total = st.f_blocks * st.f_frsize // (1024**3)
            disk_c = self._val_color(disk_pct)
            p.setFont(smb); p.setPen(WHITE); p.drawText(x, y + 12, "DISK")
            p.setFont(sm);  p.setPen(disk_c)
            p.drawText(x + 40, y + 12,
                       f"{disk_used}G / {disk_total}G  {disk_pct:.0f}%")
            self._bar(p, x, y + 15, bw, 7, disk_pct, disk_c)
            y += 30
        except Exception:
            y += 6

        p.setPen(QPen(DIM, 1)); p.drawLine(x, y, x + bw, y); y += 10

        # ── Network ──────────────────────────────────────────────
        y = self._section(p, x, y, W, "NETWORK")
        p.setFont(sm)
        p.setPen(WHITE);  p.drawText(x, y + 12, f"iface: {self._iface}")
        p.setPen(GREEN);  p.drawText(x, y + 26, f"↓ {self._rx} KB/s")
        p.setPen(CYAN);   p.drawText(x + 100, y + 26, f"↑ {self._tx} KB/s")
        y += 40
        p.setPen(QPen(DIM, 1)); p.drawLine(x, y, x + bw, y); y += 10

        # ── Top processes ─────────────────────────────────────────
        y = self._section(p, x, y, W, "TOP PROCESSES")
        p.setFont(QFont("JetBrains Mono", 8))
        p.setPen(DIM)
        p.drawText(x, y + 10, f"{'NAME':<18} {'CPU':>5} {'MEM':>5}")
        y += 13
        p.setPen(QPen(DIM, 1)); p.drawLine(x, y, x + bw, y); y += 4
        for name, cpu, mem in self._procs:
            cc = self._val_color(cpu)
            p.setPen(WHITE); p.setFont(QFont("JetBrains Mono", 8))
            p.drawText(x, y + 10, f"{name:<18}")
            p.setPen(cc)
            p.drawText(x + 145, y + 10, f"{cpu:>4.1f}%")
            p.setPen(GREEN)
            p.drawText(x + 195, y + 10, f"{mem:>4.1f}%")
            y += 13
        y += 6
        p.setPen(QPen(DIM, 1)); p.drawLine(x, y, x + bw, y); y += 10

        # ── Keybinds ─────────────────────────────────────────────
        y = self._section(p, x, y, W, "QUICK KEYS")
        keys = [
            ("Super+Return", "terminal"),
            ("Super+d",      "launcher"),
            ("Super+Ctrl+6", "restart deck"),
            ("Super+Ctrl+e", "edit graph"),
            ("Super+q",      "close window"),
            ("PrtSc",        "screenshot"),
        ]
        for k, v in keys:
            p.setFont(QFont("JetBrains Mono", 8))
            p.setPen(CYAN);  p.drawText(x, y + 11, k)
            p.setPen(DIM);   p.drawText(x + 130, y + 11, f"» {v}")
            y += 13

        p.end()
