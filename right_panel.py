"""right_panel.py — system stats shelf + app launcher / settings buttons"""
import os, json, glob, shlex, subprocess
from PySide6.QtWidgets import (
    QWidget, QPushButton, QVBoxLayout, QHBoxLayout, QLabel,
    QLineEdit, QScrollArea, QFrame,
)
from PySide6.QtCore import Qt, QTimer, QRect
from PySide6.QtGui import QPainter, QColor, QFont, QPen, QFontMetrics

from pipes_layer import PipesLayer

CFG_DIR        = os.path.expanduser("~/.config/animated-wallpaper")
APPS_JSON      = os.path.join(CFG_DIR, "apps.json")
NET_IFACE_FILE = os.path.join(CFG_DIR, "net_iface")
GLAVA_MODE_FILE  = os.path.join(CFG_DIR, "glava.mode")
GLAVA_THEME_FILE = os.path.join(CFG_DIR, "glava.theme")
HERE      = os.path.dirname(os.path.abspath(__file__))

DEFAULT_APPS = [
    {"name": "Terminal", "cmd": "ghostty"},
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
        color: %s; background: rgba(62, 68, 81, 150); text-align: left;
        border: 1px solid #3e4451; border-radius: 3px;
        font: bold 9pt 'JetBrains Mono'; padding: 4px 10px;
    }
    QPushButton:hover { background: rgba(97, 175, 239, 210); border-color: #61afef; }
"""
LBL_QSS = "color: #98c379; font: bold 9pt 'JetBrains Mono'; padding-top: 4px;"


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

CYAN   = QColor(97, 175, 239)    # One Dark blue
GREEN  = QColor(152, 195, 121)   # One Dark green
AMBER  = QColor(229, 192, 123)   # One Dark yellow
RED    = QColor(224, 108, 117)   # One Dark red
DIM    = QColor(62, 68, 81)      # One Dark dark gray
WHITE  = QColor(171, 178, 191)   # One Dark foreground
BG     = QColor(40, 44, 52, 215) # One Dark bg with alpha


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
        t.start(2500)  # stats every 2.5s (was 2s)
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
        # Create scroll area for the dock to handle overflow
        self._dock_scroll = QScrollArea(self)
        self._dock_scroll.setWidgetResizable(True)
        self._dock_scroll.setFrameShape(QScrollArea.NoFrame)
        self._dock_scroll.setStyleSheet("QScrollArea { background: transparent; border: none; }")
        self._dock_scroll.viewport().setStyleSheet("background: transparent;")
        self._dock_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self._dock_scroll.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        
        # The actual dock content widget
        self._dock = QWidget()
        self._dock.setAttribute(Qt.WA_TranslucentBackground)
        self._dock.setStyleSheet("background: transparent;")
        self._dock_scroll.setWidget(self._dock)
        
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
                color: #abb2bf; background: rgba(62, 68, 81, 150);
                border: 1px solid #3e4451; border-radius: 3px;
                font: 9pt 'JetBrains Mono'; padding: 3px 8px;
            }
            QLineEdit:focus { border-color: #61afef; }
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

        # ── GLava ON/OFF (always visible, outside collapsible) ──
        glava_sep = QFrame(self._dock)
        glava_sep.setFrameShape(QFrame.HLine)
        glava_sep.setStyleSheet("color: #c678dd;")
        lay.addWidget(glava_sep)

        self._glava_btn = button("", self._toggle_glava, "#c678dd")
        lay.addWidget(self._glava_btn)
        self._update_glava_label()

        # ── SETTINGS (collapsible) ───────────────────────────────
        self._settings_toggle = button("▸ SETTINGS", self._toggle_settings, "#c8dcff")
        lay.addWidget(self._settings_toggle)

        self._settings_holder = QWidget()
        self._settings_holder.setStyleSheet("background: transparent;")
        settings_lay = QVBoxLayout(self._settings_holder)
        settings_lay.setContentsMargins(0, 0, 0, 0)
        settings_lay.setSpacing(4)

        for b in (
            button("  Edit graph",    lambda: self._launch(os.path.join(HERE, "graph-edit.sh")), parent=self._settings_holder),
            button("  Audio mixer",   lambda: self._launch("pavucontrol"), parent=self._settings_holder),
            button("  Choose network…", self._choose_network, parent=self._settings_holder),
            button("  Wifi radio on/off", self._toggle_wifi, parent=self._settings_holder),
            button("  Mute on/off",   lambda: self._launch(
                "pactl set-sink-mute @DEFAULT_SINK@ toggle"), parent=self._settings_holder),
        ):
            settings_lay.addWidget(b)
        self._redshift_btn = button("", self._toggle_redshift, "#e5c07b", parent=self._settings_holder)
        settings_lay.addWidget(self._redshift_btn)
        self._update_redshift_label()

        # ── GLava controls section ───────────────────────────────
        glava_sep = QFrame(self._settings_holder)
        glava_sep.setFrameShape(QFrame.HLine)
        glava_sep.setStyleSheet("color: #c678dd;")
        settings_lay.addWidget(glava_sep)

        self._glava_mode_btn = button("", self._cycle_glava_mode, "#61afef", parent=self._settings_holder)
        settings_lay.addWidget(self._glava_mode_btn)
        self._update_glava_mode_label()

        self._glava_theme_btn = button("", self._cycle_glava_theme, "#98c379", parent=self._settings_holder)
        settings_lay.addWidget(self._glava_theme_btn)
        self._update_glava_theme_label()

        for b in (
            button("  Config folder", lambda: self._launch(f"xdg-open {CFG_DIR}"), parent=self._settings_holder),
            button("  Restart deck",  self._restart_deck, "#e5c07b", parent=self._settings_holder),
        ):
            settings_lay.addWidget(b)

        self._settings_scroll = QScrollArea(self._dock)
        self._settings_scroll.setWidget(self._settings_holder)
        self._settings_scroll.setWidgetResizable(True)
        self._settings_scroll.setFrameShape(QScrollArea.NoFrame)
        self._settings_scroll.setStyleSheet("QScrollArea { background: transparent; border: none; }")
        self._settings_scroll.viewport().setStyleSheet("background: transparent;")
        self._settings_scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self._settings_scroll.setVisible(False)
        lay.addWidget(self._settings_scroll)

        self._dock.adjustSize()
        self._place_dock()

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
            "☀ Normal colors" if on else "  Reading mode (5000K)")

    def _toggle_redshift(self):
        if os.path.exists(self._REDSHIFT_FLAG):
            self._launch("redshift -x")
            os.remove(self._REDSHIFT_FLAG)
        else:
            self._launch("redshift -O 5000")
            open(self._REDSHIFT_FLAG, "w").close()
        self._update_redshift_label()

    # ── GLava controls ────────────────────────────────────────────
    _GLAVA_PID_FILE = os.path.join(CFG_DIR, "glava.pid")
    GLAVA_MODES = ["graph", "bars", "wave", "radial"]
    GLAVA_THEMES = ["cyan", "green", "purple", "red", "rainbow", "sunset"]

    def _read_file(self, path, default=""):
        try:
            return open(path).read().strip()
        except Exception:
            return default

    def _is_glava_running(self):
        if not os.path.exists(self._GLAVA_PID_FILE):
            return False
        try:
            pid = int(open(self._GLAVA_PID_FILE).read().strip())
            import signal
            os.kill(pid, 0)
            return True
        except (ValueError, ProcessLookupError, PermissionError):
            return False

    def _update_glava_label(self):
        running = self._is_glava_running()
        self._glava_btn.setText("  Audio Visualizer: ON" if running else "  Audio Visualizer: OFF")

    def _toggle_glava(self):
        subprocess.Popen(
            [os.path.join(HERE, "cyberdesk.sh"), "glava-toggle"],
            start_new_session=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        QTimer.singleShot(500, self._update_glava_label)

    def _update_glava_mode_label(self):
        mode = self._read_file(GLAVA_MODE_FILE, "graph")
        self._glava_mode_btn.setText(f"  Design: {mode}")

    def _cycle_glava_mode(self):
        current = self._read_file(GLAVA_MODE_FILE, "graph")
        try:
            idx = self.GLAVA_MODES.index(current)
        except ValueError:
            idx = 0
        next_mode = self.GLAVA_MODES[(idx + 1) % len(self.GLAVA_MODES)]
        subprocess.Popen(
            [os.path.join(HERE, "cyberdesk.sh"), "glava-mode", next_mode],
            start_new_session=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        QTimer.singleShot(300, self._update_glava_mode_label)

    def _update_glava_theme_label(self):
        theme = self._read_file(GLAVA_THEME_FILE, "cyan")
        self._glava_theme_btn.setText(f"  Theme: {theme}")

    def _cycle_glava_theme(self):
        current = self._read_file(GLAVA_THEME_FILE, "cyan")
        try:
            idx = self.GLAVA_THEMES.index(current)
        except ValueError:
            idx = 0
        next_theme = self.GLAVA_THEMES[(idx + 1) % len(self.GLAVA_THEMES)]
        subprocess.Popen(
            [os.path.join(HERE, "cyberdesk.sh"), "glava-theme", next_theme],
            start_new_session=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        QTimer.singleShot(300, self._update_glava_theme_label)

    # ── Settings toggle ──────────────────────────────────────────
    _settings_open = False

    def _toggle_settings(self):
        self._settings_open = not self._settings_open
        if self._settings_open:
            # Show 8 settings items visible, scrollable if more
            btn_h = 26  # approximate button height
            spacing = 4
            visible_items = 8
            settings_h = visible_items * btn_h + (visible_items - 1) * spacing
            self._settings_scroll.setFixedHeight(settings_h)
        self._settings_scroll.setVisible(self._settings_open)
        self._settings_toggle.setText(
            "▾ SETTINGS" if self._settings_open else "▸ SETTINGS")
        self._dock.adjustSize()
        self._place_dock()

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
            # Delete both scroll area and dock
            if hasattr(self, '_dock_scroll'):
                self._dock_scroll.deleteLater()
            if hasattr(self, '_dock'):
                self._dock.deleteLater()
            self._build_dock()
            self._place_dock()

    def _place_dock(self):
        self._dock.adjustSize()
        dw = self.width() - 24
        max_height = self.height() - 24
        scroll_height = min(self._dock.height(), max_height)
        y = self.height() - scroll_height - 12
        y = max(12, y)
        scroll_height = min(scroll_height, self.height() - y - 12)
        self._dock_scroll.setGeometry(12, y, dw, max(scroll_height, 0))

    def resizeEvent(self, e):
        self._place_dock()
        super().resizeEvent(e)

    def _update_stats(self):
        self._cpu          = _cpu_percent()
        self._mem_used, self._mem_total = _mem()
        self._temp         = _cpu_temp()
        # Top processes: only update every 5s (every 2nd call at 2500ms interval)
        self._stats_counter = getattr(self, '_stats_counter', 0) + 1
        if self._stats_counter % 2 == 0:
            self._procs = _top_procs(10)
        self.update()

    # ── helpers ──────────────────────────────────────────────────
    def _bar(self, p, x, y, bw, bh, pct, color):
        p.setPen(QPen(DIM, 1))
        p.drawRect(x, y, bw, bh)
        fill = int(bw * min(pct, 100) / 100)
        # Segmented LCD/LED bar effect
        seg_w = 4
        seg_gap = 1
        seg_x = x + 1
        while seg_x < x + 1 + fill - seg_gap:
            seg_fill_w = min(seg_w, x + 1 + fill - seg_gap - seg_x)
            p.fillRect(seg_x, y + 1, seg_fill_w, bh - 1, color)
            seg_x += seg_w + seg_gap

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
