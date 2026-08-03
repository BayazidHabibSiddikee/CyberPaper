#pragma once

#include <QDialog>
#include <QString>

class QProcess;
class QLabel;
class QPushButton;

// Network sharing, delegated to the `swordshare` helper script.
//
// Unlike the archive and conversion helpers, which run once and exit, the share
// server is long-lived: it keeps listening until the user presses Stop. That
// makes the dialog itself the owner of the process — closing the dialog stops
// the server, so there is no way to leave one running invisibly.
//
// It is an HTTP server rather than FTP because every current phone browser
// dropped ftp:// support, so a QR code containing an FTP link simply fails to
// open. It binds to the LAN address only and demands a generated password.
class ShareDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShareDialog(const QString &path, QWidget *parent = nullptr);
    ~ShareDialog() override;

private:
    void onReady();
    void onFailed(const QString &message);
    void stopServer();

    QString m_path;
    QProcess *m_proc = nullptr;
    QString m_qrFile;

    QLabel *m_qr = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_url = nullptr;
    QLabel *m_pass = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_stop = nullptr;
};

// True when `path` can be shared (it exists and is readable).
bool isShareable(const QString &path);
