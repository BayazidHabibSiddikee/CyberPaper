#pragma once
#include <QString>
#include <QStringList>

// Archive create/extract, shelled out to the usual CLI tools.
//
// Every call passes arguments as a list rather than a shell string, so a file
// named `foo; rm -rf ~`.txt is handled as one literal argument.

struct ArchiveFormat {
    QString id;        // "zip", "tar.gz", ...
    QString label;     // menu text
    QString suffix;    // appended to the archive name
};

// Formats whose creating tool is actually installed, in menu order.
QList<ArchiveFormat> availableFormats();

// True if `path` looks like an archive we know how to extract.
bool isArchive(const QString &path);

// Compress `paths` into `archivePath`. All inputs must share a parent, which
// becomes the working directory so stored entries are relative, not absolute.
// On failure returns false and fills `error`.
bool compressTo(const QStringList &paths, const QString &archivePath,
                const QString &formatId, QString *error);

// Extract `archivePath` into `destDir`.
bool extractTo(const QString &archivePath, const QString &destDir, QString *error);

// Suggested archive base name: the file's own name for one item, else the
// parent folder's name.
QString suggestedArchiveName(const QStringList &paths);
