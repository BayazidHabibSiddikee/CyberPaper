"""
left_panel.py — Matrix rain + cycling animations drawn entirely in PySide6.
No xterm, no xwinwrap. Pure Python widget.
"""
import os, random, math
from PySide6.QtWidgets import QWidget, QStackedLayout
from PySide6.QtCore import Qt, QTimer, QRect, QPointF
from PySide6.QtGui import QPainter, QColor, QFont, QPen, QLinearGradient

GLAVA_MODE = os.environ.get("CYBERDECK_GLAVA") == "1"

# ── Matrix rain ───────────────────────────────────────────────────────────────
MATRIX_CHARS = (
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789"
    "@#$%&*+=-|/\\<>[]{}^~"
)

class MatrixRain(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setAutoFillBackground(False)

        self.char_w = 14
        self.char_h = 18
        self.cols = 0
        self.drops = []      # y position (in chars) per column
        self.bright = []     # brightness per column (head glow)
        self.trail = []      # list of (col, row, alpha) fade trail cells
        self.cell_chars = [] # current char per column

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(50)  # 20 fps

    def resizeEvent(self, e):
        self.cols = max(1, self.width() // self.char_w)
        self.drops = [random.randint(-40, 0) for _ in range(self.cols)]
        self.bright = [1.0] * self.cols
        self.cell_chars = [random.choice(MATRIX_CHARS) for _ in range(self.cols)]
        self.trail = []

    def _tick(self):
        self.cols = max(1, self.width() // self.char_w)
        rows = max(1, self.height() // self.char_h)

        # Extend if needed
        while len(self.drops) < self.cols:
            self.drops.append(random.randint(-40, 0))
            self.bright.append(1.0)
            self.cell_chars.append(random.choice(MATRIX_CHARS))

        # Age existing trail
        self.trail = [(c, r, a * 0.82) for c, r, a in self.trail if a > 0.03]

        for col in range(self.cols):
            row = self.drops[col]
            if 0 <= row < rows:
                self.trail.append((col, row, 0.95))
                # Randomly mutate char
                if random.random() < 0.08:
                    self.cell_chars[col] = random.choice(MATRIX_CHARS)

            self.drops[col] += 1
            if self.drops[col] > rows + random.randint(5, 20):
                self.drops[col] = random.randint(-30, -5)

        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, False)

        # Dark semi-transparent background
        p.fillRect(self.rect(), QColor(2, 10, 5, 210))

        font = QFont("monospace", 10)
        font.setBold(False)
        p.setFont(font)

        rows = max(1, self.height() // self.char_h)

        # Draw trail
        for col, row, alpha in self.trail:
            x = col * self.char_w
            y = row * self.char_h
            # green with alpha
            g = int(200 * alpha)
            b = int(80 * alpha)
            p.setPen(QColor(0, g, b, int(255 * alpha)))
            char = self.cell_chars[col] if col < len(self.cell_chars) else "0"
            p.drawText(x, y + self.char_h - 2, char)

        # Draw bright heads
        font.setBold(True)
        p.setFont(font)
        for col in range(min(self.cols, len(self.drops))):
            row = self.drops[col]
            if 0 <= row < rows:
                x = col * self.char_w
                y = row * self.char_h
                # Bright white-green head
                p.setPen(QColor(180, 255, 200, 240))
                char = self.cell_chars[col] if col < len(self.cell_chars) else "0"
                p.drawText(x, y + self.char_h - 2, char)

        p.end()


# ── Pipes animation ───────────────────────────────────────────────────────────
PIPE_CHARS = {
    'h': '━', 'v': '┃',
    'tl': '┏', 'tr': '┓', 'bl': '┗', 'br': '┛',
    'cross': '╋',
}
PIPE_COLORS = [
    QColor(0, 212, 255), QColor(0, 255, 200), QColor(0, 255, 100),
    QColor(255, 200, 0), QColor(255, 100, 0), QColor(200, 0, 255),
]

class PipesWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.cell_w = 16
        self.cell_h = 16
        self.grid = {}   # (col,row) -> (char, QColor)
        self.pipes = []  # active pipes: {col,row,dir,color,len}

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(80)
        self._frame = 0

    def _cols(self): return max(1, self.width() // self.cell_w)
    def _rows(self): return max(1, self.height() // self.cell_h)

    def _tick(self):
        self._frame += 1
        cols, rows = self._cols(), self._rows()

        # Spawn new pipe occasionally
        if len(self.pipes) < 8 and random.random() < 0.15:
            self.pipes.append({
                'col': random.randint(0, cols - 1),
                'row': random.randint(0, rows - 1),
                'dir': random.choice(['h', 'v']),
                'color': random.choice(PIPE_COLORS),
                'len': 0,
            })

        # Age grid (fade slowly)
        dead = []
        for k in list(self.grid):
            ch, col_c, age = self.grid[k]
            if age <= 0:
                dead.append(k)
            else:
                self.grid[k] = (ch, col_c, age - 1)
        for k in dead:
            del self.grid[k]

        # Move pipes
        dead_pipes = []
        for pipe in self.pipes:
            c, r = pipe['col'], pipe['row']
            d = pipe['dir']
            ch = PIPE_CHARS['h'] if d == 'h' else PIPE_CHARS['v']
            self.grid[(c, r)] = (ch, pipe['color'], 60)
            pipe['len'] += 1

            # Maybe turn
            if random.random() < 0.12:
                old_d = d
                pipe['dir'] = 'v' if d == 'h' else 'h'
                corner_map = {('h','v'): PIPE_CHARS['tl'], ('v','h'): PIPE_CHARS['br']}
                corner = corner_map.get((old_d, pipe['dir']), PIPE_CHARS['cross'])
                self.grid[(c, r)] = (corner, pipe['color'], 60)

            # Move
            if pipe['dir'] == 'h':
                pipe['col'] = (c + random.choice([-1, 1])) % cols
            else:
                pipe['row'] = (r + random.choice([-1, 1])) % rows

            if pipe['len'] > random.randint(20, 60):
                dead_pipes.append(pipe)

        for p in dead_pipes:
            if p in self.pipes:
                self.pipes.remove(p)

        # Clear occasionally
        if self._frame % 400 == 0:
            self.grid.clear()

        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        p.fillRect(self.rect(), QColor(2, 5, 15, 220))
        font = QFont("monospace", 11)
        p.setFont(font)
        for (c, r), (ch, color, age) in self.grid.items():
            alpha = min(255, age * 4)
            col = QColor(color.red(), color.green(), color.blue(), alpha)
            p.setPen(col)
            p.drawText(c * self.cell_w, r * self.cell_h + self.cell_h - 2, ch)
        p.end()


# ── Cycling left panel (matrix → pipes → starfield) ──────────────────────────
class LeftPanel(QWidget):
    """Cycles through animations every 45 seconds."""
    CYCLE_MS = 45_000

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)

        # In glava mode the audio spectrum window sits on top of this panel,
        # so the matrix rain shows behind/through the visualizer.
        self._stack = QStackedLayout(self)
        self._stack.setContentsMargins(0, 0, 0, 0)

        self._matrix = MatrixRain(self)
        self._pipes = PipesWidget(self)

        self._stack.addWidget(self._matrix)
        self._stack.addWidget(self._pipes)
        self._stack.setCurrentIndex(0)

        self._cycle_timer = QTimer(self)
        self._cycle_timer.timeout.connect(self._next_anim)
        self._cycle_timer.start(self.CYCLE_MS)
        self._idx = 0

    def _next_anim(self):
        self._idx = (self._idx + 1) % self._stack.count()
        self._stack.setCurrentIndex(self._idx)

