#pragma once
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QList>

struct AppHandler {
    QString desktopId;   // e.g. mpv.desktop
    QString name;        // display name
    QString exec;        // Exec= line
    QString iconName;
    bool isDefault = false;

    QIcon icon() const;
};

// Apps that declare they handle this file's MIME type (Thunar-style).
QList<AppHandler> appsForFile(const QString &path);

// Preferred/default handler (mimeapps.list, then first match).
AppHandler defaultAppForFile(const QString &path);

// Launch via desktop Exec line. Returns false if launch failed.
bool openWithApp(const AppHandler &app, const QString &path);

// Open with default app; for videos prefers real players if needed.
bool openWithDefault(const QString &path);

// Known video player desktop ids / names for fallback ordering.
QStringList preferredVideoPlayers();
