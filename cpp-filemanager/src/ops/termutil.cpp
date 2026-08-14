#include "ops/termutil.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>

static QString shellQuote(const QString &s) {
    QString out = s;
    out.replace('\'', "'\\''");
    return "'" + out + "'";
}

static bool startDetached(const QString &cmd, const QStringList &args) {
    return QProcess::startDetached(cmd, args);
}

void openTerminalAt(const QString &path) {
    QFileInfo fi(path);
    QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();

    struct Try { QString cmd; QStringList args; };
    const QList<Try> tries = {
        {"ghostty", {"--working-directory=" + dir}},
        {"kitty", {"--directory", dir}},
        {"alacritty", {"--working-directory", dir}},
        {"wezterm", {"start", "--cwd", dir}},
        {"foot", {"--working-directory=" + dir}},
        {"konsole", {"--workdir", dir}},
        {"xfce4-terminal", {"--working-directory=" + dir}},
        {"gnome-terminal", {"--working-directory=" + dir}},
        {"xterm", {"-e", QString("cd %1 && exec $SHELL").arg(shellQuote(dir))}},
    };
    for (const auto &t : tries) {
        if (startDetached(t.cmd, t.args))
            return;
    }
}

bool openInYazi(const QString &path) {
    if (QStandardPaths::findExecutable("yazi").isEmpty())
        return false;

    QFileInfo fi(path);
    if (!fi.exists())
        return false;

    const QString entry = fi.absoluteFilePath();
    const QString dir = fi.isDir() ? entry : fi.absolutePath();
    const QString yaziCmd = QString("yazi %1").arg(shellQuote(entry));

    struct Try { QString cmd; QStringList args; };
    const QList<Try> tries = {
        {"ghostty", {"--working-directory=" + dir, "-e", "yazi", entry}},
        {"kitty", {"--directory", dir, "yazi", entry}},
        {"alacritty", {"--working-directory", dir, "-e", "yazi", entry}},
        {"wezterm", {"start", "--cwd", dir, "--", "yazi", entry}},
        {"foot", {"--working-directory=" + dir, "yazi", entry}},
        {"konsole", {"--workdir", dir, "-e", "yazi", entry}},
        {"xfce4-terminal", {"--working-directory=" + dir, "-e", yaziCmd}},
        {"gnome-terminal", {"--working-directory=" + dir, "--", "yazi", entry}},
        {"xterm", {"-e", QString("cd %1 && exec yazi %2")
                             .arg(shellQuote(dir), shellQuote(entry))}},
    };
    for (const auto &t : tries) {
        if (startDetached(t.cmd, t.args))
            return true;
    }
    return false;
}

bool isPreviewableFile(const QString &path) {
    QFileInfo fi(path);
    if (!fi.isFile())
        return false;

    QMimeDatabase db;
    const QString mime = db.mimeTypeForFile(fi).name();
    if (mime.startsWith("text/") || mime.startsWith("image/"))
        return true;

    // Common text-like / previewable types without text/ mime
    static const QStringList extra = {
        "application/json", "application/xml", "application/javascript",
        "application/x-shellscript", "application/x-desktop",
        "application/x-yaml", "inode/x-empty",
    };
    if (extra.contains(mime))
        return true;

    const QString ext = fi.suffix().toLower();
    static const QStringList textExt = {
        "md", "txt", "log", "conf", "cfg", "ini", "toml", "yaml", "yml",
        "json", "xml", "csv", "rs", "py", "cpp", "h", "hpp", "c", "cc",
        "js", "ts", "tsx", "jsx", "css", "html", "sh", "bash", "zsh",
        "go", "java", "kt", "lua", "rb", "php", "sql", "vim", "nix",
    };
    static const QStringList imgExt = {
        "png", "jpg", "jpeg", "gif", "webp", "bmp", "svg", "ico",
        "tif", "tiff", "avif", "heic", "jxl",
    };
    return textExt.contains(ext) || imgExt.contains(ext);
}
