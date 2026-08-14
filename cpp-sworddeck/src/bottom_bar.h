#pragma once
#include <QWidget>
#include <QVector>

struct Workspace {
    QString name;
    bool focused;
    bool urgent;

    bool operator==(const Workspace &o) const {
        return name == o.name && focused == o.focused && urgent == o.urgent;
    }
};

class BottomBar : public QWidget {
    Q_OBJECT
public:
    explicit BottomBar(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;

private slots:
    void refresh();
    void refreshWorkspaces();

private:
    struct Stats {
        int cpu, mem, disk;
        QString net, music, uptime, wifi, bat, vol;
        int batPct;
    };

    Stats m_stats;
    QVector<Workspace> m_ws;
    QVector<QPair<int,int>> m_wsRects;
};
