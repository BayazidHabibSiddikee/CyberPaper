#pragma once
#include <QWidget>
#include <QTimer>
#include <QEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include "pipes_layer.h"

class RightPanel : public QWidget {
    Q_OBJECT
public:
    explicit RightPanel(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
    void updateStats();
    void checkAppsChanged();
    void filterApps(const QString &text);

private:
    void buildDock();
    void placeDock();
    void grabXFocus();
    void releaseXFocus();
    void launch(const QString &cmd);
    void toggleWifi();
    void chooseNetwork();
    void toggleRedshift();
    void toggleGlava();
    void restartDeck();

    void drawBar(QPainter &p, int x, int y, int bw, int bh, double pct, const QColor &color);
    void drawSection(QPainter &p, int x, int y, int w, const QString &label);
    QColor valColor(double pct) const;

    PipesLayer m_pipes;
    double m_cpu = 0;
    int m_memUsed = 0, m_memTotal = 1;
    int m_temp = 0;
    bool m_hasTemp = false;
    QList<QPair<QString, QPair<double,double>>> m_procs;
    int m_statsBottom = 200;
    int m_statsCounter = 0;

    QWidget *m_dock = nullptr;
    QVBoxLayout *m_dockLayout = nullptr;
    QLineEdit *m_appSearch = nullptr;
    QScrollArea *m_appScroll = nullptr;
    QList<QPair<QPushButton*, QString>> m_appButtons;
    QPushButton *m_redshiftBtn = nullptr;
    QPushButton *m_glavaBtn = nullptr;
    qint64 m_appsMtime = 0;
    QTimer m_statsTimer;
    QTimer m_appsTimer;
};
