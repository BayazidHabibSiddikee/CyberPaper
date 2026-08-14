#pragma once

#include <QDialog>
#include <QString>

class QProcess;
class QLabel;
class QPushButton;
class QJsonObject;

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
//
// Knowing the password is not enough: exactly one device may be connected, and
// the desktop has to approve its address first. Anyone who glimpsed the QR code
// would otherwise be as authorised as your own phone, with no sign they were
// there. stdout carries events from the helper, stdin carries the answers.
class ShareDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShareDialog(const QString &path, QWidget *parent = nullptr);
    ~ShareDialog() override;

private:
    void onOutput();
    void handleEvent(const QJsonObject &o);
    void onFailed(const QString &message);
    void stopServer();
    void send(const char *command);
    void showPairing(const QString &ip);
    void showConnected(const QString &ip);

    QString m_path;
    QProcess *m_proc = nullptr;
    QString m_qrFile;

    QLabel *m_qr = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_url = nullptr;
    QLabel *m_pass = nullptr;
    QLabel *m_hint = nullptr;
    QWidget *m_pairRow = nullptr;
    QPushButton *m_allow = nullptr;
    QPushButton *m_deny = nullptr;
    QPushButton *m_disconnect = nullptr;
    QPushButton *m_stop = nullptr;
};

// True when `path` can be shared (it exists and is readable).
bool isShareable(const QString &path);
