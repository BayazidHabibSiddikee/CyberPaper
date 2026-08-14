#include "cyberdeck.h"
#include "main_panel.h"
#include "right_panel.h"
#include "bottom_bar.h"
#include "spectrum_overlay.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QTimer>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cstdlib>
#include <string>

static bool s_glavaMode = false;

CyberDeck::CyberDeck(int sw, int sh, QWidget *parent)
    : QWidget(parent), m_sw(sw), m_sh(sh),
      m_main(nullptr), m_right(nullptr), m_bottom(nullptr), m_spectrum(nullptr)
{
    if (std::getenv("CYBERDECK_GLAVA"))
        s_glavaMode = (std::string(std::getenv("CYBERDECK_GLAVA")) == "1");

    setWindowFlags(Qt::FramelessWindowHint | Qt::BypassWindowManagerHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setGeometry(0, 0, sw, sh);
    setWindowTitle("cyberdeck");

    connect(&m_lowerTimer, &QTimer::timeout, this, &CyberDeck::lowerBelow);
    m_lowerTimer.start(1000);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *colsWidget = new QWidget(this);
    colsWidget->setAttribute(Qt::WA_TranslucentBackground);
    auto *cols = new QHBoxLayout(colsWidget);
    cols->setContentsMargins(0, 0, 0, 0);
    cols->setSpacing(0);

    int lw = sw * LEFT_PCT / 100;
    int rw = sw * RIGHT_PCT / 100;
    int cw = sw - lw - rw;
    int topH = sh - BOTTOM_H;

    m_main = new MainPanel(colsWidget);
    m_right = new RightPanel(colsWidget);
    m_main->setFixedSize(lw + cw, topH);
    m_right->setFixedSize(rw, topH);
    cols->addWidget(m_main);
    cols->addWidget(m_right);

    if (s_glavaMode) {
        m_spectrum = new SpectrumOverlay(this);
        m_spectrum->setGeometry(0, 0, lw + cw, topH);
    }

    m_bottom = new BottomBar();
    m_bottom->setWindowFlags(Qt::FramelessWindowHint);
    m_bottom->setAttribute(Qt::WA_X11NetWmWindowTypeDock);
    m_bottom->setAttribute(Qt::WA_ShowWithoutActivating);
    m_bottom->setFixedSize(sw, BOTTOM_H);
    m_bottom->move(0, sh - BOTTOM_H);
    m_bottom->setWindowTitle("sworddeck-bar");

    root->addWidget(colsWidget);
    root->addStretch(1);

    if (m_spectrum) m_spectrum->raise();
}

void CyberDeck::showBottomBar() {
    if (m_bottom) m_bottom->show();
}

void CyberDeck::lowerBelow() {
    // Only lowers this window. glava's DESKTOP/BELOW properties are set once at
    // startup by cyberdesk.sh; X11 properties are durable, so re-applying them
    // every tick meant opening a display connection and walking the whole window
    // tree once a second to write values that never changed.
    lower();
}

void CyberDeck::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(40, 44, 52, 255));
}

void CyberDeck::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape && e->modifiers() & Qt::ControlModifier)
        QApplication::quit();
}

void CyberDeck::showEvent(QShowEvent *e) {
    QWidget::showEvent(e);
    QTimer::singleShot(300, this, &CyberDeck::applyX11Hints);
}

void CyberDeck::applyX11Hints() {
    setupX11Below(winId());
}

void CyberDeck::setupX11Below(WId wid) {
    Display *d = XOpenDisplay(nullptr);
    if (!d) return;
    Atom wmState = XInternAtom(d, "_NET_WM_STATE", False);
    Atom states[3] = {
        XInternAtom(d, "_NET_WM_STATE_BELOW", False),
        XInternAtom(d, "_NET_WM_STATE_SKIP_TASKBAR", False),
        XInternAtom(d, "_NET_WM_STATE_SKIP_PAGER", False)
    };
    XChangeProperty(d, wid, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char*)states, 3);
    XFlush(d);
    XCloseDisplay(d);
    qInfo("[cyberdeck] X11 BELOW hints applied (window %lu)", wid);
}
