#pragma once
#include <QWidget>
#include <QTimer>
#include <QPixmap>
#include <QPushButton>
#include "pipes_layer.h"

class MainPanel : public QWidget {
    Q_OBJECT
public:
    explicit MainPanel(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void loadGraph();
    void editGraph();
    void openFM();

private:
    PipesLayer m_pipes;
    QPixmap m_graphPixmap;
    qint64 m_mtime = 0;
    int m_nodes = 0;
    int m_edges = 0;
    QPushButton *m_editBtn;
    QPushButton *m_fmBtn;
};
