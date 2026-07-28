"""right_panel.py — system stats shelf + app launcher / settings buttons"""
import os, json, glob, shlex, subprocess
from PySide6.QtWidgets import (
    QWidget, QPushButton, QVBoxLayout, QHBoxLayout, QLabel,
    QLineEdit, QScrollArea,
)
from PySide6.QtCore import Qt, QTimer, QRect
from PySide6.QtGui import QPainter, QColor, QFont, QPen, QFontMetrics

from pipes_layer import PipesLayer

CFG_DIR        = os.path.expanduser("~/.config/animated-wallpaper")
APPS_JSON      = os.path.join(CFG_DIR, "apps.json")
NET_IFACE_FILE = os.path.join(CFG_DIR, "net_iface")
HERE      = os.path.dirname(os.path.abspath(__file__))

DEFAULT_APPS = [
    {"name": "Terminal", "cmd": "alacritty"},
    {"name": "Browser",  "cmd": "zen-browser"},
    {"name": "Files",    "cmd": "nautilus"},
]

# Standard XDG locations for .desktop launchers — covers apps installed
# system-wide, per-user, and via most package formats (deb/rpm/flatpak
# exports here too).
DESKTOP_DIRS = [
    "/usr/share/applications",
    "/usr/local/share/applications",
    "/var/lib/flatpak/exports/share/applications",
    os.path.expanduser("~/.local/share/flatpak/exports/share/applications"),
    os.path.expanduser("~/.local/share/applications"),
]

_FIELD_CODES = {"%f", "%F", "%u", "%U", "%d", "%D", "%n", "%N",
                 "%i", "%c", "%k", "%v", "%m"}


def _clean_exec(exec_line):
    """Strip desktop-entry field codes (%U, %f, ...) from an Exec= line."""
    try:
        parts = shlex.split(exec_line, posix=True)
    except ValueError:
        parts = exec_line.split()
    return " ".join(p for p in parts if p not in _FIELD_CODES)


def _scan_desktop_apps():
    """Return every visible GUI app on the system, from .desktop files."""
    seen = {}
    for d in DESKTOP_DIRS:
        for path in sorted(glob.glob(os.path.join(d, "*.desktop"))):
            try:
                name = exec_ = None
                is_app, no_display = False, False
                for line in open(path, encoding="utf-8", errors="ignore"):
                    line = line.strip()
                    if line == "[Desktop Entry]":
                        is_app = True
                        continue
                    if line.startswith("[") and line != "[Desktop Entry]":
                        break   # only read the main [Desktop Entry] group
                    if not is_app:
                        continue
                    if line.startswith("Name=") and name is None:
                        name = line.split("=", 1)[1].strip()
                    elif line.startswith("Exec="):
                        exec_ = line.split("=", 1)[1].strip()
                    elif line.startswith("NoDisplay=true") or line.startswith("Hidden=true"):
                        no_display = True
                    elif line.startswith("Type=") and line.split("=", 1)[1].strip() != "Application":
                        no_display = True
                if no_display or not name or not exec_:
                    continue
                cmd = _clean_exec(exec_)
                if cmd and name not in seen:
                    seen[name] = cmd
            except Exception:
                continue
    return [{"name": n, "cmd": c} for n, c in sorted(seen.items(), key=lambda kv: kv[0].lower())]

BTN_QSS = """
    QPushButton {
        color: %s; background: rgba(0, 30, 45, 150); text-align: left;
        border: 1px solid #1a3a5a; border-radius: 3px;
        font: bold 9pt 'JetBrains Mono'; padding: 4px 10px;
    }
    QPushButton:hover { background: rgba(0, 80, 100, 210); border-color: #00d4ff; }
"""
LBL_QSS = "color: #00ffc8; font: bold 9pt 'JetBrains Mono'; padding-top: 4px;"


def _load_pinned():
    try:
        return json.load(open(APPS_JSON))
    except Exception:
        os.makedirs(CFG_DIR, exist_ok=True)
        json.dump(DEFAULT_APPS, open(APPS_JSON, "w"), indent=2)
        return list(DEFAULT_APPS)


def _load_apps():
    """Pinned apps (apps.json) first, then every other installed GUI app."""
    pinned = _load_pinned()
    apps = list(pinned)
    pinned_names = {a.get("name") for a in pinned}
    for app in _scan_desktop_apps():
        if app["name"] not in pinned_names:
            apps.append(app)
    return apps

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


def _top_procs(n=10):
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
        self._pipes = PipesLayer(self)
        self._cpu   = 0.0
        self._mem_used = 0
        self._mem_total = 1
        self._temp = None
        self._procs = []
        self._stats_bottom = 200   # where painted stats end; dock is placed below this

        t = QTimer(self)
        t.timeout.connect(self._update_stats)
        t.start(2000)
        self._update_stats()

        self._build_dock()
        try:
            self._apps_mtime = os.path.getmtime(APPS_JSON)
        except Exception:
            self._apps_mtime = 0
        ta = QTimer(self)
        ta.timeout.connect(self._check_apps_changed)
        ta.start(3000)

    # ── App launcher / settings dock (bottom of the panel) ──────────
    def _build_dock(self):
        self._dock = QWidget(self)
        self._dock.setAttribute(Qt.WA_TranslucentBackground)
        lay = QVBoxLayout(self._dock)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(4)

        def label(text):
            l = QLabel(text, self._dock)
            l.setStyleSheet(LBL_QSS)
            lay.addWidget(l)
            return l

        def button(text, cb, color="#c8dcff", parent=None):
            b = QPushButton(text, parent or self._dock)
            b.setCursor(Qt.PointingHandCursor)
            b.setStyleSheet(BTN_QSS % color)
            b.clicked.connect(cb)
            return b

        # ── APPS header + search, side by side ───────────────────
        head = QWidget(self._dock)
        head.setAttribute(Qt.WA_TranslucentBackground)
        hl = QHBoxLayout(head)
        hl.setContentsMargins(0, 0, 0, 0)
        hl.setSpacing(6)
        apps_lbl = QLabel("── APPS ──", head)
        apps_lbl.setStyleSheet(LBL_QSS)
        hl.addWidget(apps_lbl)
        self._app_search = QLineEdit(head)
        self._app_search.setPlaceholderText("🔍 search apps…")
        self._app_search.setStyleSheet("""
            QLineEdit {
                color: #c8dcff; background: rgba(0, 30, 45, 150);
                border: 1px solid #1a3a5a; border-radius: 3px;
                font: 9pt 'JetBrains Mono'; padding: 3px 8px;
            }
            QLineEdit:focus { border-color: #00d4ff; }
        """)
        self._app_search.textChanged.connect(self._filter_apps)
        # The deck window is override-redirect, so X never focuses it on its
        # own — grab/release X input focus when the box is clicked/left.
        self._app_search.installEventFilter(self)
        hl.addWidget(self._app_search, 1)
        lay.addWidget(head)

        # ── Scrollable, searchable list of every installed GUI app ─
        apps_holder = QWidget()
        apps_holder.setAutoFillBackground(False)
        apps_holder.setStyleSheet("background: transparent;")
        apps_lay = QVBoxLayout(apps_holder)
        apps_lay.setContentsMargins(0, 0, 4, 0)
        apps_lay.setSpacing(4)

        self._app_buttons = []   # (QPushButton, lowercase-name) for filtering
        for app in _load_apps():
            name = app.get("name") or app.get("cmd", "")
            cmd = app.get("cmd", "")
            b = button(f"▸ {name}", lambda _=False, c=cmd: self._launch(c),
                       parent=apps_holder)
            apps_lay.addWidget(b)
            self._app_buttons.append((b, name.lower()))
        apps_lay.addStretch(1)

        # Show exactly 8 app rows; the rest scroll. Height is derived from
        # the actual button size (not guessed), so it stays accurate
        # regardless of font/DPI.
        VISIBLE_APPS = 8
        if self._app_buttons:
            btn_h = self._app_buttons[0][0].sizeHint().height()
        else:
            btn_h = 26
        list_h = VISIBLE_APPS * btn_h + (VISIBLE_APPS - 1) * apps_lay.spacing()

        scroll = QScrollArea(self._dock)
        scroll.setWidget(apps_holder)
        scroll.setWidgetResizable(True)
        scroll.setFixedHeight(list_h)
        scroll.setFrameShape(QScrollArea.NoFrame)
        scroll.setStyleSheet("QScrollArea { background: transparent; border: none; }")
        scroll.viewport().setStyleSheet("background: transparent;")
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self._app_scroll = scroll
        lay.addWidget(scroll)

        label("── SETTINGS ──")
        for b in (
            button("✎ Edit graph",    lambda: self._launch(os.path.join(HERE, "graph-edit.sh"))),
            button("♪ Audio mixer",   lambda: self._launch("pavucontrol")),
            button("📶 Choose network…", self._choose_network),
            button("⇄ Wifi radio on/off", self._toggle_wifi),
            button("🔇 Mute on/off",   lambda: self._launch(
                "pactl set-sink-mute @DEFAULT_SINK@ toggle")),
        ):
            lay.addWidget(b)
        self._redshift_btn = button("", self._toggle_redshift, "#ffc800")
        lay.addWidget(self._redshift_btn)
        self._update_redshift_label()
        for b in (
            button("⚙ Config folder", lambda: self._launch(f"xdg-open {CFG_DIR}")),
            button("⟳ Restart deck",  self._restart_deck, "#ffc800"),
        ):
            lay.addWidget(b)

        self._dock.adjustSize()

    # ── X input focus for the search box ─────────────────────────
    # The deck is an override-redirect window: i3 never assigns keyboard
    # focus to it, so a plain QLineEdit receives clicks but no keystrokes.
    def _grab_x_focus(self):
        try:
            import Xlib.display, Xlib.X
            d = Xlib.display.Display()
            w = d.create_resource_object("window", int(self.window().winId()))
            w.set_input_focus(Xlib.X.RevertToPointerRoot, Xlib.X.CurrentTime)
            d.sync()
            d.close()
        except Exception:
            pass

    def _release_x_focus(self):
        try:
            import Xlib.display, Xlib.X
            d = Xlib.display.Display()
            d.set_input_focus(Xlib.X.PointerRoot,
                              Xlib.X.RevertToPointerRoot, Xlib.X.CurrentTime)
            d.sync()
            d.close()
        except Exception:
            pass

    def eventFilter(self, obj, e):
        if obj is self._app_search:
            from PySide6.QtCore import QEvent
            if e.type() == QEvent.MouseButtonPress:
                self._grab_x_focus()
                self._app_search.setFocus(Qt.MouseFocusReason)
            elif e.type() == QEvent.KeyPress and e.key() == Qt.Key_Escape:
                self._app_search.clearFocus()
                self._release_x_focus()
                return True
            elif e.type() == QEvent.FocusOut:
                self._release_x_focus()
        return super().eventFilter(obj, e)

    def _filter_apps(self, text):
        needle = text.strip().lower()
        for btn, name_lower in self._app_buttons:
            btn.setVisible(not needle or needle in name_lower)
        self._app_scroll.verticalScrollBar().setValue(0)

    def _launch(self, cmd):
        if cmd:
            subprocess.Popen(cmd, shell=True, start_new_session=True,
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def _toggle_wifi(self):
        self._launch(
            'state=$(nmcli radio wifi); '
            'if [ "$state" = "enabled" ]; then nmcli radio wifi off; '
            'else nmcli radio wifi on; fi')

    def _choose_network(self):
        # Lists every saved connection (wifi, ethernet, mobile broadband /
        # modem — anything NetworkManager knows about) plus nearby wifi
        # networks that aren't saved yet, via rofi. Picking a saved
        # connection just brings it up; picking a new SSID prompts for a
        # password (leave blank for open networks) and connects to it.
        script = r'''
sel=$( { nmcli -t -f NAME,TYPE connection show 2>/dev/null \
           | awk -F: '{printf "%s  [%s]\n", $1, $2}';
         nmcli -t -f SSID,SIGNAL dev wifi list 2>/dev/null \
           | awk -F: '$1!="" {printf "%s  (wifi %s%%)\n", $1, $2}'; } \
       | sort -u \
       | rofi -dmenu -p "Connect to:" -i )
[ -z "$sel" ] && exit 0
name=$(echo "$sel" | sed -E 's/  \[[^]]*\]$//; s/  \(wifi[^)]*\)$//')
if nmcli -t -f NAME connection show | grep -Fxq "$name"; then
    # Saved connection — works the same for wifi, ethernet, or a modem
    nmcli connection up id "$name"
else
    pass=$(rofi -dmenu -p "Password (blank = open network):" -password </dev/null)
    if [ -z "$pass" ]; then
        nmcli device wifi connect "$name"
    else
        nmcli device wifi connect "$name" password "$pass"
    fi
fi
'''
        subprocess.Popen(["bash", "-c", script], start_new_session=True,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # redshift toggle: reading mode (warm 5000K) <-> normal (-x)
    _REDSHIFT_FLAG = os.path.join(CFG_DIR, "redshift.on")

    def _update_redshift_label(self):
        on = os.path.exists(self._REDSHIFT_FLAG)
        self._redshift_btn.setText(
            "☀ Normal colors" if on else "☾ Reading mode (5000K)")

    def _toggle_redshift(self):
        if os.path.exists(self._REDSHIFT_FLAG):
            self._launch("redshift -x")
            os.remove(self._REDSHIFT_FLAG)
        else:
            self._launch("redshift -O 5000")
            open(self._REDSHIFT_FLAG, "w").close()
        self._update_redshift_label()

    def _restart_deck(self):
        subprocess.Popen([os.path.join(HERE, "cyberdesk.sh"), "restart"],
                         start_new_session=True,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def _check_apps_changed(self):
        try:
            mt = os.path.getmtime(APPS_JSON)
        except Exception:
            return
        if mt != self._apps_mtime:
            self._apps_mtime = mt
            self._dock.deleteLater()
            self._build_dock()
            self._dock.show()
            self._place_dock()

    def _place_dock(self):
        self._dock.adjustSize()
        dw = self.width() - 24
        # Prefer anchoring to the bottom (original look), but never let the
        # dock creep up higher than the painted stats block actually ends —
        # this is what caused "QUICK KEYS" to overlap the app search box.
        bottom_anchor = self.height() - self._dock.height() - 12
        top_anchor    = self._stats_bottom + 10
        y = max(bottom_anchor, top_anchor)
        self._dock.setGeometry(12, y, dw, self._dock.height())

    def resizeEvent(self, e):
        self._place_dock()
        super().resizeEvent(e)

    def _update_stats(self):
        self._cpu          = _cpu_percent()
        self._mem_used, self._mem_total = _mem()
        self._temp         = _cpu_temp()
        self._procs        = _top_procs(10)
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
        # no fill — shared bluish background comes from the deck window

        # ── Bottom layer: animated pipes texture (also runs in left_panel.py) ──
        self._pipes.paint(p)

        x   = 12
        bw  = W - 24
        y   = 10
        sm  = QFont("JetBrains Mono", 9)
        smb = QFont("JetBrains Mono", 9, QFont.Bold)

        # ── Header ───────────────────────────────────────────────
        f = QFont("JetBrains Mono", 16, QFont.Bold)
        p.setFont(f); p.setPen(CYAN)
        hw = QFontMetrics(f).horizontalAdvance("▸ SWORDDECK")
        p.drawText((W - hw) // 2, y + 20, "▸ SWORDDECK")
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

        if abs(y - self._stats_bottom) > 1:
            self._stats_bottom = y
            self._place_dock()

        p.end()
