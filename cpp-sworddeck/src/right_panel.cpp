#include "right_panel.h"

#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QPen>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QCoreApplication>
#include <QRegularExpression>
#include <sys/statvfs.h>
#include <unistd.h>
#include <csignal>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#undef KeyPress
#undef FocusOut

static const QColor CYAN(97, 175, 239);
static const QColor GREEN(152, 195, 121);
static const QColor AMBER(229, 192, 123);
static const QColor RED(224, 108, 117);
static const QColor DIM(62, 68, 81);
static const QColor WHITE(171, 178, 191);

static QString configDir() { return QDir::homePath() + "/.config/animated-wallpaper"; }

static double cpuPercent() {
    static long long prevTotal = 0, prevIdle = 0;
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
    QString line = QString::fromLocal8Bit(f.readLine());
    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() < 8) return 0;
    long long total = 0;
    for (int i = 1; i < 8; i++) total += parts[i].toLongLong();
    long long idle = parts[4].toLongLong();
    long long dt = total - prevTotal, di = idle - prevIdle;
    prevTotal = total; prevIdle = idle;
    return dt ? 100.0 * (1.0 - (double)di / dt) : 0;
}

static void memInfo(int &used, int &total) {
    used = 0; total = 1;
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QMap<QString, long> info;
    while (!f.atEnd()) {
        QString l = f.readLine();
        auto kv = l.split(':');
        if (kv.size() == 2)
            info[kv[0].trimmed()] = kv[1].trimmed().split(' ')[0].toLong();
    }
    total = info["MemTotal"] / 1024;
    used = (info["MemTotal"] - info["MemAvailable"]) / 1024;
}

static int diskPercent() {
    struct statvfs st;
    if (statvfs("/", &st) != 0) return 0;
    return (int)(100.0 * (1.0 - (double)st.f_bavail / st.f_blocks));
}

static int cpuTemp() {
    for (int hw = 0; hw < 8; hw++)
        for (int t = 1; t < 5; t++) {
            QFile f(QString("/sys/class/hwmon/hwmon%1/temp%2_input").arg(hw).arg(t));
            if (f.open(QIODevice::ReadOnly)) {
                int val = f.readAll().trimmed().toInt() / 1000;
                if (val > 20 && val < 120) return val;
            }
        }
    return 0;
}

static QList<QPair<QString, QPair<double,double>>> topProcs(int n = 10) {
    QList<QPair<QString, QPair<double,double>>> result;
    QProcess proc;
    proc.start("ps", {"aux", "--sort=-%cpu"});
    proc.waitForFinished(2000);
    QStringList lines = QString::fromLocal8Bit(proc.readAllStandardOutput()).split('\n');
    for (int i = 1; i < qMin(lines.size(), n + 1); i++) {
        QStringList parts = lines[i].split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 11)
            result.append({parts[10].left(18), {parts[2].toDouble(), parts[3].toDouble()}});
    }
    return result;
}

RightPanel::RightPanel(QWidget *parent)
    : QWidget(parent), m_pipes(this)
{
    setAttribute(Qt::WA_TranslucentBackground);

    connect(&m_statsTimer, &QTimer::timeout, this, &RightPanel::updateStats);
    m_statsTimer.start(2500);
    updateStats();

    buildDock();

    QFileInfo fi(configDir() + "/apps.json");
    m_appsMtime = fi.lastModified().toSecsSinceEpoch();

    connect(&m_appsTimer, &QTimer::timeout, this, &RightPanel::checkAppsChanged);
    m_appsTimer.start(3000);
}

void RightPanel::buildDock() {
    if (m_dock) m_dock->deleteLater();

    m_dock = new QWidget(this);
    m_dock->setAttribute(Qt::WA_TranslucentBackground);
    m_dockLayout = new QVBoxLayout(m_dock);
    m_dockLayout->setContentsMargins(0, 0, 0, 0);
    m_dockLayout->setSpacing(4);

    auto lbl = [&](const QString &text) {
        auto *l = new QLabel(text, m_dock);
        l->setStyleSheet("color: #98c379; font: bold 9pt 'JetBrains Mono'; padding-top: 4px;");
        m_dockLayout->addWidget(l);
    };

    auto btn = [&](const QString &text, auto slot, const QString &color = "#c8dcff") {
        auto *b = new QPushButton(text, m_dock);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton { color: %1; background: rgba(62, 68, 81, 150); "
            "text-align: left; border: 1px solid #3e4451; border-radius: 3px; "
            "font: bold 9pt 'JetBrains Mono'; padding: 4px 10px; } "
            "QPushButton:hover { background: rgba(97, 175, 239, 210); border-color: #61afef; }").arg(color));
        connect(b, &QPushButton::clicked, this, slot);
        return b;
    };

    auto *head = new QWidget(m_dock);
    head->setAttribute(Qt::WA_TranslucentBackground);
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);
    auto *appsLbl = new QLabel("── APPS ──", head);
    appsLbl->setStyleSheet("color: #98c379; font: bold 9pt 'JetBrains Mono';");
    hl->addWidget(appsLbl);

    m_appSearch = new QLineEdit(head);
    m_appSearch->setPlaceholderText("🔍 search apps…");
    m_appSearch->setStyleSheet(
        "QLineEdit { color: #abb2bf; background: rgba(62, 68, 81, 150); "
        "border: 1px solid #3e4451; border-radius: 3px; "
        "font: 9pt 'JetBrains Mono'; padding: 3px 8px; }"
        "QLineEdit:focus { border-color: #61afef; }"
    );
    connect(m_appSearch, &QLineEdit::textChanged, this, &RightPanel::filterApps);
    m_appSearch->installEventFilter(this);
    hl->addWidget(m_appSearch, 1);
    m_dockLayout->addWidget(head);

    auto *appsHolder = new QWidget();
    appsHolder->setAutoFillBackground(false);
    appsHolder->setStyleSheet("background: transparent;");
    auto *appsLay = new QVBoxLayout(appsHolder);
    appsLay->setContentsMargins(0, 0, 4, 0);
    appsLay->setSpacing(4);

    QStringList apps = {"Terminal", "Browser", "Files"};
    QFile appsFile(configDir() + "/apps.json");
    if (appsFile.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(appsFile.readAll());
        QStringList loaded;
        for (const auto &a : doc.array())
            loaded.append(a.toObject()["name"].toString());
        if (!loaded.isEmpty()) apps = loaded;
    }

    for (const auto &name : apps) {
        auto *b = new QPushButton(QString("▸ %1").arg(name), appsHolder);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet("QPushButton { color: #c8dcff; background: rgba(62, 68, 81, 150); "
            "text-align: left; border: 1px solid #3e4451; border-radius: 3px; "
            "font: bold 9pt 'JetBrains Mono'; padding: 4px 10px; } "
            "QPushButton:hover { background: rgba(97, 175, 239, 210); border-color: #61afef; }");
        connect(b, &QPushButton::clicked, [this, name]() { launch(name.toLower()); });
        appsLay->addWidget(b);
        m_appButtons.append({b, name.toLower()});
    }
    appsLay->addStretch(1);

    m_appScroll = new QScrollArea(m_dock);
    m_appScroll->setWidget(appsHolder);
    m_appScroll->setWidgetResizable(true);
    m_appScroll->setFixedHeight(8 * 26 + 7 * 4);
    m_appScroll->setFrameShape(QScrollArea::NoFrame);
    m_appScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_appScroll->viewport()->setStyleSheet("background: transparent;");
    m_appScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_dockLayout->addWidget(m_appScroll);

    lbl("── SETTINGS ──");
    btn("  Edit graph", [this]() { launch(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../graph-edit.sh")); });
    btn("  Audio mixer", [this]() { launch("pavucontrol"); });
    btn("  Choose network…", [this]() { chooseNetwork(); });
    btn("  Wifi radio on/off", [this]() { toggleWifi(); });
    btn("  Mute on/off", [this]() { launch("pactl set-sink-mute @DEFAULT_SINK@ toggle"); });
    m_redshiftBtn = btn("  Reading mode (5000K)", [this]() { toggleRedshift(); }, "#e5c07b");
    m_glavaBtn = btn("  Audio Visualizer: OFF", [this]() { toggleGlava(); }, "#c678dd");
    btn("  Config folder", [this]() { launch("xdg-open " + configDir()); });
    btn("  Restart deck", [this]() { restartDeck(); }, "#e5c07b");

    m_dock->adjustSize();
}

void RightPanel::placeDock() {
    m_dock->adjustSize();
    int dw = width() - 24;
    int bottomAnchor = height() - m_dock->height() - 12;
    int topAnchor = m_statsBottom + 10;
    int y = qMax(bottomAnchor, topAnchor);
    m_dock->setGeometry(12, y, dw, m_dock->height());
}

void RightPanel::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    placeDock();
}

bool RightPanel::eventFilter(QObject *obj, QEvent *e) {
    if (obj == m_appSearch) {
        if (e->type() == QEvent::MouseButtonPress) {
            grabXFocus();
            m_appSearch->setFocus(Qt::MouseFocusReason);
        } else if (e->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent*>(e);
            if (ke->key() == Qt::Key_Escape) {
                m_appSearch->clearFocus();
                releaseXFocus();
                return true;
            }
        } else if (e->type() == QEvent::FocusOut) {
            releaseXFocus();
        }
    }
    return QWidget::eventFilter(obj, e);
}

void RightPanel::grabXFocus() {
    Display *d = XOpenDisplay(nullptr);
    if (!d) return;
    XSetInputFocus(d, winId(), RevertToPointerRoot, CurrentTime);
    XFlush(d);
    XCloseDisplay(d);
}

void RightPanel::releaseXFocus() {
    Display *d = XOpenDisplay(nullptr);
    if (!d) return;
    XSetInputFocus(d, PointerRoot, RevertToPointerRoot, CurrentTime);
    XFlush(d);
    XCloseDisplay(d);
}

void RightPanel::filterApps(const QString &text) {
    QString needle = text.trimmed().toLower();
    for (auto &[btn, name] : m_appButtons)
        btn->setVisible(needle.isEmpty() || name.contains(needle));
    if (m_appScroll)
        m_appScroll->verticalScrollBar()->setValue(0);
}

void RightPanel::launch(const QString &cmd) {
    if (!cmd.isEmpty())
        QProcess::startDetached("sh", {"-c", cmd});
}

void RightPanel::toggleWifi() {
    launch("state=$(nmcli radio wifi); "
           "if [ \"$state\" = \"enabled\" ]; then nmcli radio wifi off; "
           "else nmcli radio wifi on; fi");
}

void RightPanel::chooseNetwork() {
    launch(R"(
sel=$( { nmcli -t -f NAME,TYPE connection show 2>/dev/null \
           | awk -F: '{printf "%s  [%s]\n", $1, $2}';
         nmcli -t -f SSID,SIGNAL dev wifi list 2>/dev/null \
           | awk -F: '$1!="" {printf "%s  (wifi %s%%)\n", $1, $2}'; } \
       | sort -u \
       | rofi -dmenu -p "Connect to:" -i )
[ -z "$sel" ] && exit 0
name=$(echo "$sel" | sed -E 's/  \[[^]]*\]$//; s/  \(wifi[^)]*\)$//')
if nmcli -t -f NAME connection show | grep -Fxq "$name"; then
    nmcli connection up id "$name"
else
    pass=$(rofi -dmenu -p "Password (blank = open network):" -password </dev/null)
    if [ -z "$pass" ]; then
        nmcli device wifi connect "$name"
    else
        nmcli device wifi connect "$name" password "$pass"
    fi
fi
)");
}

void RightPanel::toggleRedshift() {
    QString flag = configDir() + "/redshift.on";
    if (QFile::exists(flag)) {
        launch("redshift -x");
        QFile::remove(flag);
        m_redshiftBtn->setText("  Reading mode (5000K)");
    } else {
        launch("redshift -O 5000");
        QFile f(flag);
        f.open(QIODevice::WriteOnly);
        f.close();
        m_redshiftBtn->setText("☀ Normal colors");
    }
}

void RightPanel::toggleGlava() {
    launch(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../cyberdesk.sh") + " glava-toggle");
    QTimer::singleShot(500, [this]() {
        QString pidFile = configDir() + "/glava.pid";
        if (QFile::exists(pidFile)) {
            QFile f(pidFile);
            if (f.open(QIODevice::ReadOnly)) {
                int pid = QString::fromLocal8Bit(f.readAll()).trimmed().toInt();
                if (kill(pid, 0) == 0) {
                    m_glavaBtn->setText("  Audio Visualizer: ON");
                    return;
                }
            }
        }
        m_glavaBtn->setText("  Audio Visualizer: OFF");
    });
}

void RightPanel::restartDeck() {
    launch(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../cyberdesk.sh") + " restart");
}

void RightPanel::checkAppsChanged() {
    QFileInfo fi(configDir() + "/apps.json");
    qint64 mt = fi.lastModified().toSecsSinceEpoch();
    if (mt != m_appsMtime) {
        m_appsMtime = mt;
        buildDock();
        m_dock->show();
        placeDock();
    }
}

void RightPanel::updateStats() {
    m_cpu = cpuPercent();
    memInfo(m_memUsed, m_memTotal);
    m_temp = cpuTemp();
    m_hasTemp = (m_temp > 0);
    m_statsCounter++;
    if (m_statsCounter % 2 == 0)
        m_procs = topProcs(10);
    update();
}

void RightPanel::drawBar(QPainter &p, int x, int y, int bw, int bh, double pct, const QColor &color) {
    p.setPen(QPen(DIM, 1));
    p.drawRect(x, y, bw, bh);
    int fill = (int)(bw * qMin(pct, 100.0) / 100.0);
    int segW = 4, segGap = 1, segX = x + 1;
    while (segX < x + 1 + fill - segGap) {
        int segFillW = qMin(segW, x + 1 + fill - segGap - segX);
        p.fillRect(segX, y + 1, segFillW, bh - 1, color);
        segX += segW + segGap;
    }
}

void RightPanel::drawSection(QPainter &p, int x, int y, int w, const QString &label) {
    p.setPen(QPen(DIM, 1));
    p.drawLine(x, y, x + w - 8, y);
    QFont f("JetBrains Mono", 9, QFont::Bold);
    p.setFont(f);
    p.setPen(GREEN);
    p.drawText(x, y - 2, "── " + label + " ──");
}

QColor RightPanel::valColor(double pct) const {
    if (pct >= 80) return RED;
    if (pct >= 50) return AMBER;
    return GREEN;
}

void RightPanel::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int W = width(), H = height();

    m_pipes.paint(p);

    int x = 12, bw = W - 24, y = 10;
    QFont sm("JetBrains Mono", 9);
    QFont smb("JetBrains Mono", 9, QFont::Bold);

    QFont hf("JetBrains Mono", 16, QFont::Bold);
    p.setFont(hf);
    p.setPen(CYAN);
    QFontMetrics hfm(hf);
    p.drawText((W - hfm.horizontalAdvance("▸ SWORDDECK")) / 2, y + 20, "▸ SWORDDECK");
    y += 34;
    p.setPen(QPen(DIM, 1));
    p.drawLine(x, y, x + bw, y);
    y += 10;

    drawSection(p, x, y, W, "SYSTEM");
    y += 14;
    QColor cpuC = valColor(m_cpu);
    p.setFont(smb); p.setPen(WHITE); p.drawText(x, y + 12, "CPU");
    p.setFont(sm); p.setPen(cpuC);
    p.drawText(x + 35, y + 12, QString("%1%").arg(m_cpu, 0, 'f', 0));
    if (m_hasTemp) { p.setPen(AMBER); p.drawText(x + 80, y + 12, QString("  %1°C").arg(m_temp)); }
    drawBar(p, x, y + 15, bw, 7, m_cpu, cpuC);
    y += 28;

    double memPctVal = 100.0 * m_memUsed / qMax(1, m_memTotal);
    QColor memC = valColor(memPctVal);
    p.setFont(smb); p.setPen(WHITE); p.drawText(x, y + 12, "RAM");
    p.setFont(sm); p.setPen(memC);
    p.drawText(x + 35, y + 12, QString("%1M / %2M  %3%").arg(m_memUsed).arg(m_memTotal).arg(memPctVal, 0, 'f', 0));
    drawBar(p, x, y + 15, bw, 7, memPctVal, memC);
    y += 30;

    int dp = diskPercent();
    if (dp > 0) {
        QColor diskC = valColor(dp);
        p.setFont(smb); p.setPen(WHITE); p.drawText(x, y + 12, "DISK");
        p.setFont(sm); p.setPen(diskC);
        p.drawText(x + 40, y + 12, QString("%1%").arg(dp));
        drawBar(p, x, y + 15, bw, 7, dp, diskC);
        y += 30;
    }
    y += 6;

    p.setPen(QPen(DIM, 1)); p.drawLine(x, y, x + bw, y); y += 10;

    drawSection(p, x, y, W, "TOP PROCESSES");
    y += 14;
    p.setFont(QFont("JetBrains Mono", 8)); p.setPen(DIM);
    p.drawText(x, y + 10, QString("%1%2 %3%4").arg("NAME", -18).arg("CPU", 5).arg("MEM", 5));
    y += 13;
    p.setPen(QPen(DIM, 1)); p.drawLine(x, y, x + bw, y); y += 4;
    for (const auto &[name, cpu_mem] : m_procs) {
        QColor cc = valColor(cpu_mem.first);
        p.setFont(QFont("JetBrains Mono", 8)); p.setPen(WHITE);
        p.drawText(x, y + 10, QString("%1").arg(name, -18));
        p.setPen(cc); p.drawText(x + 145, y + 10, QString("%1%").arg(cpu_mem.first, 0, 'f', 1));
        p.setPen(GREEN); p.drawText(x + 195, y + 10, QString("%1%").arg(cpu_mem.second, 0, 'f', 1));
        y += 13;
    }
    y += 6;
    p.setPen(QPen(DIM, 1)); p.drawLine(x, y, x + bw, y); y += 10;

    drawSection(p, x, y, W, "QUICK KEYS");
    y += 14;
    struct { const char *key, *action; } keys[] = {
        {"Super+Return", "terminal"}, {"Super+d", "launcher"},
        {"Super+Ctrl+6", "restart deck"}, {"Super+Ctrl+e", "edit graph"},
        {"Super+q", "close window"}, {"PrtSc", "screenshot"},
    };
    for (const auto &k : keys) {
        p.setFont(QFont("JetBrains Mono", 8));
        p.setPen(CYAN); p.drawText(x, y + 11, k.key);
        p.setPen(DIM); p.drawText(x + 130, y + 11, QString("» %1").arg(k.action));
        y += 13;
    }

    if (qAbs(y - m_statsBottom) > 1) {
        m_statsBottom = y;
        placeDock();
    }
    p.end();
}
