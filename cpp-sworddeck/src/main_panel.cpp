#include "main_panel.h"

#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QDateTime>
#include <QCoreApplication>
#include <sys/utsname.h>
#include <cmath>

static const QColor CYAN(97, 175, 239);
static const QColor GREEN(152, 195, 121);
static const QColor DIM(62, 68, 81);
static const QColor WHITE(171, 178, 191);

static QString configDir() {
    return QDir::homePath() + "/.config/animated-wallpaper";
}

MainPanel::MainPanel(QWidget *parent)
    : QWidget(parent), m_pipes(this)
{
    setAttribute(Qt::WA_TranslucentBackground);
    loadGraph();

    auto *clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
    clockTimer->start(1000);

    auto *watcher = new QFileSystemWatcher({configDir() + "/graph.png", configDir() + "/graph.json"}, this);
    connect(watcher, &QFileSystemWatcher::fileChanged, this, &MainPanel::loadGraph);

    m_editBtn = new QPushButton("✚ EDIT GRAPH", this);
    m_editBtn->setCursor(Qt::PointingHandCursor);
    m_editBtn->setStyleSheet(
        "QPushButton { color: #98c379; background: rgba(62, 68, 81, 160);"
        "border: 1px solid #3e4451; border-radius: 3px;"
        "font: bold 9pt 'JetBrains Mono'; padding: 3px 10px; }"
        "QPushButton:hover { background: rgba(97, 175, 239, 200); border-color: #61afef; }"
    );
    connect(m_editBtn, &QPushButton::clicked, this, &MainPanel::editGraph);

    m_fmBtn = new QPushButton("📁 SWORDFM", this);
    m_fmBtn->setCursor(Qt::PointingHandCursor);
    m_fmBtn->setStyleSheet(
        "QPushButton { color: #98c379; background: rgba(62, 68, 81, 160);"
        "border: 1px solid #3e4451; border-radius: 3px;"
        "font: bold 9pt 'JetBrains Mono'; padding: 3px 10px; }"
        "QPushButton:hover { background: rgba(97, 175, 239, 200); border-color: #61afef; }"
    );
    connect(m_fmBtn, &QPushButton::clicked, this, &MainPanel::openFM);
}

void MainPanel::editGraph() {
    QString script = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../graph-edit.sh");
    QProcess::startDetached(script);
}

void MainPanel::openFM() {
    QProcess::startDetached("swordfm");
}

void MainPanel::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    m_editBtn->adjustSize();
    m_fmBtn->adjustSize();
    int gap = 8;
    int total = m_editBtn->width() + gap + m_fmBtn->width();
    int x = width() - total - 16;
    int y = height() - 56;
    m_editBtn->move(x, y);
    m_fmBtn->move(x + m_editBtn->width() + gap, y);
}

void MainPanel::loadGraph() {
    QString png = configDir() + "/graph.png";
    QString json = configDir() + "/graph.json";

    if (QFile::exists(png)) {
        m_graphPixmap.load(png);
        m_mtime = QFileInfo(png).lastModified().toSecsSinceEpoch();
    } else {
        m_graphPixmap = QPixmap();
    }

    QFile f(json);
    if (f.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(f.readAll());
        auto obj = doc.object();
        m_nodes = obj["nodes"].toArray().size();
        m_edges = obj["edges"].toArray().size();
    }
}

void MainPanel::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // The graph PNG is always scaled down into the panel; without this the
    // downscale is nearest-neighbour and its text turns to mush.
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    int w = width(), h = height();

    m_pipes.paint(p);

    QDateTime now = QDateTime::currentDateTime();
    QFont clockFont("JetBrains Mono", 34, QFont::Bold);
    p.setFont(clockFont);
    p.setPen(CYAN);
    QString ts = now.toString("HH:mm:ss");
    QFontMetrics fm(clockFont);
    p.drawText((w - fm.horizontalAdvance(ts)) / 2, 58, ts);

    QFont dateFont("JetBrains Mono", 11);
    p.setFont(dateFont);
    p.setPen(GREEN);
    QString ds = now.toString("ddd  dd MMM yyyy");
    QFontMetrics dfm(dateFont);
    p.drawText((w - dfm.horizontalAdvance(ds)) / 2, 80, ds);

    p.setPen(QPen(DIM, 1));
    p.drawLine(16, 95, w - 16, 95);

    int graphTop = 100;
    int graphH = h - graphTop - 60;
    if (!m_graphPixmap.isNull()) {
        // Keep the aspect ratio and centre it; stretching the PNG into the
        // panel rect distorted the graph as well as blurring it.
        const QSize scaled = m_graphPixmap.size().scaled(w, graphH, Qt::KeepAspectRatio);
        const QRect dst((w - scaled.width()) / 2,
                        graphTop + (graphH - scaled.height()) / 2,
                        scaled.width(), scaled.height());
        p.drawPixmap(dst, m_graphPixmap, m_graphPixmap.rect());
    } else {
        p.setPen(DIM);
        p.setFont(QFont("JetBrains Mono", 11));
        p.drawText(QRect(0, graphTop, w, graphH), Qt::AlignCenter, "[ no graph ]");
    }

    int sy = h - 34;
    p.setPen(QPen(DIM, 1));
    p.drawLine(16, sy, w - 16, sy);
    p.setFont(QFont("JetBrains Mono", 9));

    QString upt = "?";
    QFile uptimeFile("/proc/uptime");
    if (uptimeFile.open(QIODevice::ReadOnly)) {
        double secs = uptimeFile.readAll().split(' ')[0].toDouble();
        upt = QString("%1h %2m").arg(int(secs / 3600)).arg(int(fmod(secs, 3600) / 60));
    }

    QString kern = "?";
    struct utsname uts;
    if (uname(&uts) == 0)
        kern = QString(uts.release).split('-')[0];

    int tx = 16;
    p.setPen(GREEN);
    QString s = QString("◈ Nodes: %1   Links: %2").arg(m_nodes).arg(m_edges);
    p.drawText(tx, sy + 16, s);
    tx += fm.horizontalAdvance(s) + 30;

    p.setPen(WHITE);
    s = QString("◈ Uptime: %1").arg(upt);
    p.drawText(tx, sy + 16, s);
    tx += fm.horizontalAdvance(s) + 30;

    p.setPen(CYAN);
    p.drawText(tx, sy + 16, QString("◈ Kernel: %1").arg(kern));

    p.end();
}
