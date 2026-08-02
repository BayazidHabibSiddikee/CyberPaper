#include "pipes_layer.h"
#include <QWidget>
#include <QFont>
#include <algorithm>

static const QChar PIPE_H   = QChar(0x2501);
static const QChar PIPE_V   = QChar(0x2503);
static const QChar PIPE_TL  = QChar(0x250F);
static const QChar PIPE_BR  = QChar(0x251B);
static const QChar PIPE_CROSS = QChar(0x2533);

static const QColor PIPE_COLORS[] = {
    QColor(97, 175, 239),
    QColor(152, 195, 121),
    QColor(229, 192, 123),
    QColor(198, 120, 221),
    QColor(86, 182, 194),
    QColor(224, 108, 117),
};
static const int NUM_COLORS = sizeof(PIPE_COLORS) / sizeof(PIPE_COLORS[0]);

PipesLayer::PipesLayer(QWidget *widget, int cell, int interval, int maxAlpha, QObject *parent)
    : QObject(parent), m_widget(widget), m_cellW(cell), m_cellH(cell), m_maxAlpha(maxAlpha)
{
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PipesLayer::tick);
    timer->start(interval);
}

int PipesLayer::cols() const { return std::max(1, m_widget->width() / m_cellW); }
int PipesLayer::rows() const { return std::max(1, m_widget->height() / m_cellH); }

void PipesLayer::tick() {
    m_frame++;
    int c = cols(), r = rows();
    std::uniform_int_distribution<int> distC(0, c - 1);
    std::uniform_int_distribution<int> distR(0, r - 1);
    std::uniform_int_distribution<int> distColor(0, NUM_COLORS - 1);
    std::uniform_int_distribution<int> distLen(20, 60);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    if (m_pipes.size() < 8 && prob(m_rng) < 0.15) {
        Pipe p;
        p.col = distC(m_rng);
        p.row = distR(m_rng);
        p.dir = (prob(m_rng) < 0.5) ? 'h' : 'v';
        p.color = PIPE_COLORS[distColor(m_rng)];
        p.len = 0;
        m_pipes.append(p);
    }

    // Age grid entries
    QList<GridKey> dead;
    for (auto it = m_grid.begin(); it != m_grid.end(); ++it) {
        it->age--;
        if (it->age <= 0)
            dead.append(it.key());
    }
    for (const auto &k : dead)
        m_grid.remove(k);

    // Move pipes
    for (int i = m_pipes.size() - 1; i >= 0; i--) {
        auto &pipe = m_pipes[i];
        QChar ch = (pipe.dir == 'h') ? PIPE_H : PIPE_V;
        m_grid[{pipe.col, pipe.row}] = {ch, pipe.color, 60};
        pipe.len++;

        if (prob(m_rng) < 0.12) {
            char oldDir = pipe.dir;
            pipe.dir = (pipe.dir == 'h') ? 'v' : 'h';
            QChar corner;
            if (oldDir == 'h' && pipe.dir == 'v') corner = PIPE_TL;
            else if (oldDir == 'v' && pipe.dir == 'h') corner = PIPE_BR;
            else corner = PIPE_CROSS;
            m_grid[{pipe.col, pipe.row}] = {corner, pipe.color, 60};
        }

        if (pipe.dir == 'h')
            pipe.col = (pipe.col + ((prob(m_rng) < 0.5) ? -1 : 1) + c) % c;
        else
            pipe.row = (pipe.row + ((prob(m_rng) < 0.5) ? -1 : 1) + r) % r;

        if (pipe.len > distLen(m_rng))
            m_pipes.removeAt(i);
    }

    if (m_frame % 400 == 0)
        m_grid.clear();

    emit needsUpdate();
}

void PipesLayer::paint(QPainter &p) {
    QFont font("monospace", 11);
    p.setFont(font);
    for (auto it = m_grid.begin(); it != m_grid.end(); ++it) {
        int alpha = std::min(m_maxAlpha, it->age * 2);
        QColor col(it->color.red(), it->color.green(), it->color.blue(), alpha);
        p.setPen(col);
        p.drawText(it.key().x * m_cellW, it.key().y * m_cellH + m_cellH - 2, it->ch);
    }
}
