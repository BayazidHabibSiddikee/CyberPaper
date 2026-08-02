#pragma once
#include <QObject>
#include <QTimer>
#include <QPainter>
#include <QColor>
#include <QMap>
#include <random>

struct GridKey {
    int x, y;
    bool operator<(const GridKey &o) const {
        return (x < o.x) || (x == o.x && y < o.y);
    }
};

struct PipeChar {
    QChar ch;
    QColor color;
    int age;
};

struct Pipe {
    int col, row;
    char dir;
    QColor color;
    int len;
};

class QWidget;

class PipesLayer : public QObject {
    Q_OBJECT
public:
    explicit PipesLayer(QWidget *widget, int cell = 16, int interval = 80, int maxAlpha = 140, QObject *parent = nullptr);

    void paint(QPainter &p);

signals:
    void needsUpdate();

private slots:
    void tick();

private:
    int cols() const;
    int rows() const;

    QWidget *m_widget;
    int m_cellW, m_cellH;
    int m_maxAlpha;
    QMap<GridKey, PipeChar> m_grid;
    QList<Pipe> m_pipes;
    int m_frame = 0;
    std::mt19937 m_rng{std::random_device{}()};
};
