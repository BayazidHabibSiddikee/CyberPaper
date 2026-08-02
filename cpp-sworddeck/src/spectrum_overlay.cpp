#include "spectrum_overlay.h"

#include <QPainter>
#include <QLinearGradient>
#include <QProcess>
#include <QFile>
#include <QMutexLocker>
#include <cmath>
#include <ctime>

static const QColor CYAN_LOW(30, 60, 100);
static const QColor CYAN_HIGH(97, 175, 239);

AudioThread::AudioThread(int n, QObject *parent)
    : QThread(parent), m_n(n), m_data(n, 0.0) {}

void AudioThread::stop() { m_running = false; wait(1000); }

void AudioThread::run() {
    // Try parec
    QString monitor;
    QProcess pactl;
    pactl.start("pactl", {"get-default-sink"});
    if (pactl.waitForFinished(2000)) {
        QString sink = QString::fromLocal8Bit(pactl.readAllStandardOutput()).trimmed();
        if (!sink.isEmpty()) monitor = sink + ".monitor";
    }

    QStringList args = {"--format=float32le", "--rate=44100", "--channels=1"};
    if (!monitor.isEmpty()) args << "-d" << monitor;

    QProcess proc;
    proc.start("parec", args);
    if (proc.waitForStarted(1000)) {
        while (m_running) {
            QByteArray raw = proc.read(m_n * 4);
            if (raw.size() < m_n * 4) break;
            const float *vals = reinterpret_cast<const float*>(raw.constData());
            for (int i = 0; i < m_n; i++) {
                m_data[i] = m_data[i] * 0.6 + qMin(std::abs(vals[i]) * 4.0, 1.0) * 0.4;
            }
            emit levels(m_data);
        }
    } else {
        // Fallback: idle wave
        while (m_running) {
            double t = time(nullptr);
            for (int i = 0; i < m_n; i++) {
                double v = std::sin(t * 1.4 + i * 0.25) * 0.15 + 0.15;
                m_data[i] = m_data[i] * 0.7 + qBound(0.0, v, 1.0) * 0.3;
            }
            emit levels(m_data);
            msleep(50);
        }
    }
}

SpectrumOverlay::SpectrumOverlay(QWidget *parent, int n)
    : QWidget(parent), m_n(n), m_raw(n, 0.0), m_smooth(n, 0.0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    m_audio = new AudioThread(n, this);
    connect(m_audio, &AudioThread::levels, this, &SpectrumOverlay::onAudio);
    m_audio->start();

    connect(&m_timer, &QTimer::timeout, this, &SpectrumOverlay::tick);
    m_timer.start(33);
}

void SpectrumOverlay::onAudio(const QVector<double> &data) { m_raw = data; }

void SpectrumOverlay::tick() {
    for (int i = 0; i < m_n; i++)
        m_smooth[i] = m_smooth[i] * 0.78 + (i < m_raw.size() ? m_raw[i] : 0.0) * 0.22;
    update();
}

void SpectrumOverlay::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    int w = width(), h = height();
    int bw = qMax(4, w / m_n - 2);
    int tw = m_n * (bw + 2);
    int sx = (w - tw) / 2;

    for (int i = 0; i < m_n; i++) {
        int bh = (int)(m_smooth[i] * h * 0.55);
        if (bh <= 0) continue;
        int x = sx + i * (bw + 2);
        int y = h - bh;
        QLinearGradient g(x, y + bh, x, y);
        g.setColorAt(0.0, QColor(CYAN_LOW.red(), CYAN_LOW.green(), CYAN_LOW.blue(), 35));
        g.setColorAt(1.0, QColor(CYAN_HIGH.red(), CYAN_HIGH.green(), CYAN_HIGH.blue(), 130));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(x, y, bw, bh, 2, 2);
    }
    p.end();
}

void SpectrumOverlay::closeEvent(QCloseEvent *e) {
    m_audio->stop();
    m_audio->deleteLater();
    QWidget::closeEvent(e);
}
