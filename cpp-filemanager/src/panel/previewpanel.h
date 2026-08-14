#pragma once
#include <QWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QScrollArea>
#include <QToolButton>
#include <QProcess>

class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget *parent = nullptr);

    void previewFile(const QString &path);
    void previewGraph(const QString &folderPath);
    void clearPreview();
    QString currentPath() const { return m_path; }
    bool isEmpty() const { return m_path.isEmpty(); }

signals:
    void closeRequested();

private:
    void showEmpty();
    void showText(const QString &path);
    void showImage(const QString &path);
    void showPdf(const QString &path);
    void showBinaryStub(const QString &path, const QString &reason);
    bool loadTextFile(const QString &path, QString *out, QString *error);

    // Zoom applies to whatever pixmap is on screen — image, graph or PDF page.
    void applyZoom();
    void zoomBy(double factor);
    void zoomFit();
    void setZoomControlsVisible(bool on);

    QString m_path;
    QString m_graphPng;
    QString m_pdfPng;
    QPixmap m_sourcePixmap;
    double m_zoom = 1.0;
    bool m_zoomFit = true;
    QProcess *m_graphProc = nullptr;
    QProcess *m_pdfProc = nullptr;
    QLabel *m_title = nullptr;
    QToolButton *m_zoomOutBtn = nullptr;
    QToolButton *m_zoomInBtn = nullptr;
    QToolButton *m_zoomFitBtn = nullptr;
    QToolButton *m_closeBtn = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_emptyPage = nullptr;
    QPlainTextEdit *m_textView = nullptr;
    QLabel *m_imageLabel = nullptr;
    QScrollArea *m_imageScroll = nullptr;
    QLabel *m_stubLabel = nullptr;
};
