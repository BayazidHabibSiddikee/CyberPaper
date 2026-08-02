#include "panel/previewpanel.h"
#include "app/theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPixmap>
#include <QFont>
#include <QFontDatabase>
#include <QImageReader>
#include <QTextOption>
#include <QTextCursor>
#include <QDir>
#include <QCoreApplication>
#include <QStandardPaths>

static const qint64 kMaxTextBytes = 2 * 1024 * 1024; // 2 MB

static bool isImagePath(const QFileInfo &fi, const QString &mime) {
    if (mime.startsWith("image/"))
        return true;
    static const QStringList imgExt = {
        "png", "jpg", "jpeg", "gif", "webp", "bmp", "svg", "ico",
        "tif", "tiff", "avif", "jxl", "heic"
    };
    return imgExt.contains(fi.suffix().toLower());
}

static bool isTextish(const QFileInfo &fi, const QString &mime) {
    if (mime.startsWith("text/"))
        return true;
    static const QStringList mimeOk = {
        "application/json", "application/xml", "application/javascript",
        "application/x-shellscript", "application/x-desktop",
        "application/x-yaml", "inode/x-empty",
    };
    if (mimeOk.contains(mime))
        return true;
    static const QStringList textExt = {
        "md", "txt", "log", "conf", "cfg", "ini", "toml", "yaml", "yml",
        "json", "xml", "csv", "rs", "py", "cpp", "h", "hpp", "c", "cc",
        "js", "ts", "tsx", "jsx", "css", "html", "htm", "sh", "bash", "zsh",
        "go", "java", "kt", "lua", "rb", "php", "sql", "vim", "nix",
        "cmake", "makefile", "mk", "gradle", "dart", "swift", "r", "jl",
    };
    return textExt.contains(fi.suffix().toLower())
        || fi.fileName().compare("Makefile", Qt::CaseInsensitive) == 0
        || fi.fileName().compare("CMakeLists.txt", Qt::CaseInsensitive) == 0;
}

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(280);
    setStyleSheet(QString(
        "PreviewPanel { background: %1; border-left: 1px solid %2; }"
        "QLabel#previewTitle { color: %3; font-weight: 600; font-size: 12px; padding: 0 4px; }"
        "QLabel#previewEmpty { color: %4; font-size: 13px; }"
        "QLabel#previewStub { color: %4; font-size: 13px; padding: 16px; }"
        "QPlainTextEdit {"
        "  background: %1; color: %5; border: none;"
        "  selection-background-color: %2; selection-color: %3;"
        "}"
        "QToolButton {"
        "  background: transparent; border: none; color: %4; border-radius: 4px; padding: 2px;"
        "}"
        "QToolButton:hover { background: %2; color: %3; }"
        "QScrollArea { background: %1; border: none; }"
    ).arg(Theme::BG, Theme::DIM, Theme::CYAN, Theme::FG_DIM, Theme::FG));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QWidget(this);
    header->setFixedHeight(34);
    header->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;")
                              .arg(Theme::BG2, Theme::BORDER));
    auto *hdrLay = new QHBoxLayout(header);
    hdrLay->setContentsMargins(10, 0, 6, 0);

    m_title = new QLabel("PREVIEW", header);
    m_title->setObjectName("previewTitle");

    auto mkZoomBtn = [this, header](const QString &glyph, const QString &tip) {
        auto *b = new QToolButton(header);
        b->setText(glyph);
        b->setToolTip(tip);
        b->setFixedSize(24, 24);
        b->hide();
        return b;
    };
    m_zoomOutBtn = mkZoomBtn(QStringLiteral("−"), "Zoom out  (Ctrl+-)");
    m_zoomInBtn  = mkZoomBtn(QStringLiteral("+"), "Zoom in  (Ctrl++)");
    m_zoomFitBtn = mkZoomBtn(QStringLiteral("⤢"), "Fit to panel  (Ctrl+0)");
    connect(m_zoomOutBtn, &QToolButton::clicked, this, [this]() { zoomBy(1.0 / 1.25); });
    connect(m_zoomInBtn,  &QToolButton::clicked, this, [this]() { zoomBy(1.25); });
    connect(m_zoomFitBtn, &QToolButton::clicked, this, &PreviewPanel::zoomFit);

    m_closeBtn = new QToolButton(header);
    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setToolTip("Close preview");
    m_closeBtn->setFixedSize(24, 24);
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() {
        clearPreview();
        emit closeRequested();
    });

    hdrLay->addWidget(m_title, 1);
    hdrLay->addWidget(m_zoomOutBtn);
    hdrLay->addWidget(m_zoomInBtn);
    hdrLay->addWidget(m_zoomFitBtn);
    hdrLay->addWidget(m_closeBtn);
    root->addWidget(header);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);

    m_emptyPage = new QWidget(this);
    auto *emptyLay = new QVBoxLayout(m_emptyPage);
    auto *emptyLbl = new QLabel("Select a file to preview\ntext · code · images", m_emptyPage);
    emptyLbl->setObjectName("previewEmpty");
    emptyLbl->setAlignment(Qt::AlignCenter);
    emptyLay->addStretch();
    emptyLay->addWidget(emptyLbl);
    emptyLay->addStretch();
    m_stack->addWidget(m_emptyPage);

    m_textView = new QPlainTextEdit(this);
    m_textView->setReadOnly(true);
    m_textView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_textView->setWordWrapMode(QTextOption::NoWrap);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(11);
    m_textView->setFont(mono);
    m_stack->addWidget(m_textView);

    m_imageScroll = new QScrollArea(this);
    m_imageScroll->setWidgetResizable(true);
    m_imageScroll->setAlignment(Qt::AlignCenter);
    m_imageLabel = new QLabel;
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(QString("background: %1;").arg(Theme::BG));
    m_imageScroll->setWidget(m_imageLabel);
    m_stack->addWidget(m_imageScroll);

    m_stubLabel = new QLabel(this);
    m_stubLabel->setObjectName("previewStub");
    m_stubLabel->setAlignment(Qt::AlignCenter);
    m_stubLabel->setWordWrap(true);
    m_stack->addWidget(m_stubLabel);

    showEmpty();
}

void PreviewPanel::showEmpty() {
    m_path.clear();
    m_title->setText("PREVIEW");
    m_stack->setCurrentWidget(m_emptyPage);
    m_textView->clear();
    m_imageLabel->clear();
    m_sourcePixmap = QPixmap();
    setZoomControlsVisible(false);
}

void PreviewPanel::clearPreview() {
    showEmpty();
}

bool PreviewPanel::loadTextFile(const QString &path, QString *out, QString *error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = "Cannot open file";
        return false;
    }
    if (f.size() > kMaxTextBytes) {
        *error = QString("File too large to preview (%1 MB)")
                     .arg(f.size() / (1024.0 * 1024), 0, 'f', 1);
        return false;
    }
    QByteArray data = f.readAll();
    int nulls = 0;
    const int check = qMin(data.size(), 8192);
    for (int i = 0; i < check; ++i) {
        if (data.at(i) == '\0')
            ++nulls;
    }
    if (nulls > 0) {
        *error = "Binary file";
        return false;
    }
    *out = QString::fromUtf8(data);
    return true;
}

void PreviewPanel::showText(const QString &path) {
    QString text, err;
    if (!loadTextFile(path, &text, &err)) {
        showBinaryStub(path, err);
        return;
    }
    m_textView->setPlainText(text);
    m_textView->moveCursor(QTextCursor::Start);
    setZoomControlsVisible(false);
    m_stack->setCurrentWidget(m_textView);
}

void PreviewPanel::showImage(const QString &path) {
    QPixmap px;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (!img.isNull())
        px = QPixmap::fromImage(img);
    else
        px = QPixmap(path);

    if (px.isNull()) {
        showBinaryStub(path, "Could not load image");
        return;
    }

    // Keep the full-resolution pixmap so zooming in re-samples from the
    // original rather than magnifying an already-downscaled copy.
    m_sourcePixmap = px;
    m_zoomFit = true;
    applyZoom();
    setZoomControlsVisible(true);
    m_stack->setCurrentWidget(m_imageScroll);
}

void PreviewPanel::applyZoom() {
    if (m_sourcePixmap.isNull())
        return;

    const int maxW = qMax(120, m_imageScroll->viewport()->width() - 4);
    const int maxH = qMax(120, m_imageScroll->viewport()->height() - 4);

    QPixmap shown;
    if (m_zoomFit) {
        shown = (m_sourcePixmap.width() > maxW || m_sourcePixmap.height() > maxH)
                    ? m_sourcePixmap.scaled(maxW, maxH, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation)
                    : m_sourcePixmap;
        // Remember what "fit" resolved to so the first zoom step continues
        // from the size on screen instead of jumping to 100%.
        m_zoom = m_sourcePixmap.width() > 0
                     ? double(shown.width()) / m_sourcePixmap.width() : 1.0;
    } else {
        shown = m_sourcePixmap.scaled(m_sourcePixmap.size() * m_zoom,
                                      Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_imageScroll->setWidgetResizable(m_zoomFit);
    m_imageLabel->setPixmap(shown);
    m_imageLabel->adjustSize();
}

void PreviewPanel::zoomBy(double factor) {
    if (m_sourcePixmap.isNull())
        return;
    m_zoomFit = false;
    m_zoom = qBound(0.05, m_zoom * factor, 20.0);
    applyZoom();
}

void PreviewPanel::zoomFit() {
    if (m_sourcePixmap.isNull())
        return;
    m_zoomFit = true;
    applyZoom();
}

void PreviewPanel::setZoomControlsVisible(bool on) {
    m_zoomOutBtn->setVisible(on);
    m_zoomInBtn->setVisible(on);
    m_zoomFitBtn->setVisible(on);
}

void PreviewPanel::showBinaryStub(const QString &path, const QString &reason) {
    QFileInfo fi(path);
    setZoomControlsVisible(false);
    m_stubLabel->setText(QString("%1\n\n%2\n%3 bytes")
                             .arg(fi.fileName(), reason)
                             .arg(fi.size()));
    m_stack->setCurrentWidget(m_stubLabel);
}

void PreviewPanel::showPdf(const QString &path) {
    const QString exe = QStandardPaths::findExecutable("pdftoppm");
    if (exe.isEmpty()) {
        showBinaryStub(path, "Install poppler for PDF previews");
        return;
    }

    setZoomControlsVisible(false);
    m_stubLabel->setText(QString("Rendering %1…").arg(QFileInfo(path).fileName()));
    m_stack->setCurrentWidget(m_stubLabel);

    if (m_pdfProc) {
        m_pdfProc->kill();
        m_pdfProc->deleteLater();
    }
    // pdftoppm appends "-<page>.png" to the prefix we hand it.
    const QString prefix = QDir::temp().filePath(
        QString("swordfm-pdf-%1").arg(QCoreApplication::applicationPid()));
    m_pdfPng = prefix + "-1.png";
    QFile::remove(m_pdfPng);

    m_pdfProc = new QProcess(this);
    const QString target = QFileInfo(path).absoluteFilePath();
    connect(m_pdfProc, &QProcess::finished, this,
            [this, target](int code, QProcess::ExitStatus) {
        if (m_path != target)
            return; // selection moved on while we were rendering
        if (code == 0 && QFileInfo::exists(m_pdfPng))
            showImage(m_pdfPng);
        else
            showBinaryStub(target, "Could not render PDF");
    });
    m_pdfProc->start(exe, {"-png", "-r", "150", "-f", "1", "-l", "1",
                           target, prefix});
}

void PreviewPanel::previewGraph(const QString &folderPath) {
    QFileInfo fi(folderPath);
    if (!fi.isDir()) {
        showEmpty();
        return;
    }

    m_path = fi.absoluteFilePath();
    m_title->setText(QString("⬡ %1").arg(fi.fileName()));
    m_stubLabel->setText(QString("Building graph for\n%1…").arg(fi.fileName()));
    m_stack->setCurrentWidget(m_stubLabel);

    if (m_graphProc) {
        m_graphProc->kill();
        m_graphProc->deleteLater();
    }

    if (m_graphPng.isEmpty()) {
        m_graphPng = QDir::temp().filePath(
            QString("swordfm-graph-%1.png").arg(QCoreApplication::applicationPid()));
    }

    // swordgraph lives next to the file manager in the project tree; fall back
    // to PATH when swordfm has been installed on its own.
    QString exe = QCoreApplication::applicationDirPath() + "/../../swordgraph";
    if (!QFileInfo::exists(exe))
        exe = QStandardPaths::findExecutable("swordgraph");
    if (exe.isEmpty() || !QFileInfo::exists(exe)) {
        showBinaryStub(folderPath, "swordgraph not found");
        return;
    }

    m_graphProc = new QProcess(this);
    const QString target = m_path;
    connect(m_graphProc, &QProcess::finished, this,
            [this, target](int code, QProcess::ExitStatus) {
        if (m_path != target)
            return; // selection moved on while we were rendering
        if (code == 0 && QFileInfo::exists(m_graphPng)) {
            showImage(m_graphPng);
            m_title->setText(QString("⬡ %1").arg(QFileInfo(target).fileName()));
        } else {
            showBinaryStub(target, "Could not build graph");
        }
    });
    m_graphProc->start(exe, {"--out", m_graphPng, m_path});
}

void PreviewPanel::previewFile(const QString &path) {
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        showEmpty();
        return;
    }

    m_path = fi.absoluteFilePath();
    m_title->setText(fi.fileName());

    QMimeDatabase db;
    const QString mime = db.mimeTypeForFile(fi).name();

    if (isImagePath(fi, mime)) {
        showImage(path);
        return;
    }
    if (mime == "application/pdf" || fi.suffix().compare("pdf", Qt::CaseInsensitive) == 0) {
        showPdf(path);
        return;
    }
    if (isTextish(fi, mime)) {
        showText(path);
        return;
    }

    QString text, err;
    if (fi.size() <= kMaxTextBytes && loadTextFile(path, &text, &err)) {
        m_textView->setPlainText(text);
        m_stack->setCurrentWidget(m_textView);
        return;
    }

    showBinaryStub(path, err.isEmpty() ? "No preview available" : err);
}
