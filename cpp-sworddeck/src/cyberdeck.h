#pragma once
#include <QWidget>
#include <QTimer>

class MainPanel;
class RightPanel;
class BottomBar;
class SpectrumOverlay;

class CyberDeck : public QWidget {
    Q_OBJECT
public:
    explicit CyberDeck(int sw, int sh, QWidget *parent = nullptr);
    void showBottomBar();

protected:
    void paintEvent(QPaintEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void showEvent(QShowEvent *e) override;

private slots:
    void lowerBelow();

private:
    void applyX11Hints();
    void setupX11Below(WId wid);

    int m_sw, m_sh;
    MainPanel *m_main;
    RightPanel *m_right;
    BottomBar *m_bottom;
    SpectrumOverlay *m_spectrum;
    QTimer m_lowerTimer;
};

constexpr int LEFT_PCT   = 28;
constexpr int CENTER_PCT = 44;
constexpr int RIGHT_PCT  = 28;
constexpr int BOTTOM_H   = 32;
