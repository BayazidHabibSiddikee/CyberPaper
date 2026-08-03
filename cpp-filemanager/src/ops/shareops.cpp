#include "ops/shareops.h"
#include "app/theme.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace {

// swordshare lives beside the file manager in the project tree during
// development, and on PATH once installed.
QString helperPath() {
    const QString local =
        QFileInfo(QCoreApplication::applicationDirPath() + "/../../swordshare")
            .absoluteFilePath();
    if (QFileInfo::exists(local)) return local;
    return QStandardPaths::findExecutable("swordshare");
}

QLabel *makeLabel(const QString &text, const QString &color, int size,
                  bool mono = false) {
    auto *l = new QLabel(text);
    l->setAlignment(Qt::AlignCenter);
    l->setWordWrap(true);
    l->setStyleSheet(QString("color: %1; font-size: %2px; %3")
                         .arg(color).arg(size)
                         .arg(mono ? "font-family: monospace;" : ""));
    return l;
}

} // namespace

bool isShareable(const QString &path) {
    const QFileInfo fi(path);
    return fi.exists() && fi.isReadable();
}

ShareDialog::ShareDialog(const QString &path, QWidget *parent)
    : QDialog(parent), m_path(path) {
    setWindowTitle("Share over Network");
    setMinimumWidth(340);
    setStyleSheet(QString("QDialog { background: %1; }").arg(Theme::BG));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(22, 20, 22, 18);

    layout->addWidget(makeLabel(QFileInfo(path).fileName(), Theme::CYAN, 15));

    m_qr = new QLabel;
    m_qr->setAlignment(Qt::AlignCenter);
    m_qr->setMinimumHeight(250);
    layout->addWidget(m_qr);

    m_status = makeLabel("Starting server…", Theme::FG_DIM, 13);
    layout->addWidget(m_status);

    m_url = makeLabel(QString(), Theme::FG, 13, true);
    m_url->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_url->hide();
    layout->addWidget(m_url);

    m_pass = makeLabel(QString(), Theme::AMBER, 26, true);
    m_pass->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pass->hide();
    layout->addWidget(m_pass);

    m_hint = makeLabel(QString(), Theme::FG_DIM, 12);
    m_hint->hide();
    layout->addWidget(m_hint);

    m_stop = new QPushButton("Stop Server");
    m_stop->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none;"
                "  border-radius: 5px; padding: 9px 0; font-size: 14px; }"
                "QPushButton:hover { background: %3; }")
            .arg(Theme::DIM, Theme::RED, Theme::BG2));
    connect(m_stop, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(m_stop);

    const QString helper = helperPath();
    if (helper.isEmpty()) {
        onFailed("The swordshare helper was not found.\n"
                 "Re-run install-cpp.sh to install it.");
        return;
    }

    // Argument list, not a shell string: a folder named `a; rm -rf ~` must
    // reach the helper as one literal argument.
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &ShareDialog::onReady);
    connect(m_proc, &QProcess::errorOccurred, this,
            [this]() { onFailed("Could not start the share server."); });
    m_proc->start(helper, QStringList{path});
}

ShareDialog::~ShareDialog() { stopServer(); }

void ShareDialog::stopServer() {
    if (!m_proc) return;
    // terminate() first so the helper can delete its temporary QR file; kill
    // only if it ignores that.
    m_proc->terminate();
    if (!m_proc->waitForFinished(3000)) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
    m_proc = nullptr;
}

void ShareDialog::onReady() {
    // The helper prints exactly one JSON line when it is listening; anything
    // after that is request logging we do not care about.
    const QByteArray line = m_proc->readLine().trimmed();
    if (line.isEmpty()) return;

    const QJsonObject o = QJsonDocument::fromJson(line).object();
    if (o.isEmpty()) return;
    disconnect(m_proc, &QProcess::readyReadStandardOutput, this, nullptr);

    const QString url = o.value("url").toString();
    const QString password = o.value("password").toString();
    m_qrFile = o.value("qr").toString();

    if (!m_qrFile.isEmpty()) {
        QPixmap pm(m_qrFile);
        if (!pm.isNull()) {
            m_qr->setPixmap(pm.scaled(240, 240, Qt::KeepAspectRatio,
                                      Qt::FastTransformation));
        }
    }
    if (m_qr->pixmap().isNull()) {
        m_qr->setText("Install the python `qrcode` package\nto get a scannable code.");
        m_qr->setStyleSheet(QString("color: %1; font-size: 12px;").arg(Theme::FG_DIM));
    }

    m_status->setText("Scan with your phone, then enter the code");
    m_url->setText(url);
    m_url->show();
    m_pass->setText(password);
    m_pass->show();
    m_hint->setText(o.value("upload").toBool()
                        ? "Phone can download and upload · this network only"
                        : "Download only · this network only");
    m_hint->show();

    QGuiApplication::clipboard()->setText(url);
}

void ShareDialog::onFailed(const QString &message) {
    m_status->setText(message);
    m_status->setStyleSheet(QString("color: %1; font-size: 13px;").arg(Theme::RED));
    m_qr->hide();
    m_stop->setText("Close");
}
