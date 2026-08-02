#pragma once
#include <QWidget>
#include <QTimer>
#include <QThread>
#include <QVector>

class AudioThread : public QThread {
    Q_OBJECT
public:
    explicit AudioThread(int n = 48, QObject *parent = nullptr);
    void stop();

signals:
    void levels(const QVector<double> &data);

protected:
    void run() override;

private:
    int m_n;
    QVector<double> m_data;
    bool m_running = true;
};

class SpectrumOverlay : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumOverlay(QWidget *parent = nullptr, int n = 48);

protected:
    void paintEvent(QPaintEvent *e) override;
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onAudio(const QVector<double> &data);
    void tick();

private:
    int m_n;
    QVector<double> m_raw;
    QVector<double> m_smooth;
    AudioThread *m_audio;
    QTimer m_timer;
};
