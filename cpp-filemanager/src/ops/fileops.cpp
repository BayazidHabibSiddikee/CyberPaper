#include "ops/fileops.h"

#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDirIterator>

QString uniqueDestPath(const QString &destDir, const QString &fileName) {
    QString dest = destDir + "/" + fileName;
    if (!QFileInfo::exists(dest))
        return dest;

    QFileInfo fi(fileName);
    QString base = fi.completeBaseName();
    QString ext = fi.suffix();
    QString suffix = ext.isEmpty() ? QString() : ("." + ext);

    // Handle "file.tar.gz"-ish: if base empty use full name
    if (base.isEmpty())
        base = fileName;

    for (int i = 1; i < 10000; ++i) {
        QString candidate = QString("%1/%2 (%3)%4").arg(destDir).arg(base).arg(i).arg(suffix);
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return dest;
}

bool copyFiles(const QStringList &srcPaths, const QString &destDir) {
    bool ok = true;
    QDir().mkpath(destDir);

    for (const auto &src : srcPaths) {
        QFileInfo fi(src);
        if (!fi.exists()) continue;

        // Don't copy into itself
        if (fi.isDir() && destDir.startsWith(fi.absoluteFilePath()))
            continue;

        QString dest = uniqueDestPath(destDir, fi.fileName());

        if (fi.isDir()) {
            if (!QDir().mkpath(dest)) {
                ok = false;
                continue;
            }
            QDirIterator it(src, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QString relPath = QDir(src).relativeFilePath(it.filePath());
                QString destPath = dest + "/" + relPath;
                if (it.fileInfo().isDir()) {
                    QDir().mkpath(destPath);
                } else {
                    QDir().mkpath(QFileInfo(destPath).absolutePath());
                    if (QFileInfo::exists(destPath))
                        QFile::remove(destPath);
                    if (!QFile::copy(it.filePath(), destPath))
                        ok = false;
                }
            }
        } else {
            if (QFileInfo::exists(dest))
                QFile::remove(dest);
            if (!QFile::copy(src, dest))
                ok = false;
        }
    }
    return ok;
}

bool moveFiles(const QStringList &srcPaths, const QString &destDir) {
    bool ok = true;
    QDir().mkpath(destDir);

    for (const auto &src : srcPaths) {
        QFileInfo fi(src);
        if (!fi.exists()) continue;

        QString absSrc = fi.absoluteFilePath();
        if (fi.absolutePath() == QFileInfo(destDir).absoluteFilePath())
            continue;
        if (fi.isDir() && destDir.startsWith(absSrc))
            continue;

        QString dest = uniqueDestPath(destDir, fi.fileName());
        if (!QFile::rename(absSrc, dest)) {
            // Cross-device: copy then delete
            if (copyFiles({absSrc}, destDir)) {
                if (!deleteFileOrDir(absSrc))
                    ok = false;
            } else {
                ok = false;
            }
        }
    }
    return ok;
}

bool deleteFileOrDir(const QString &path) {
    QFileInfo fi(path);
    if (!fi.exists()) return false;
    if (fi.isDir())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}

qint64 selectedTotalSize(const QStringList &paths) {
    qint64 total = 0;
    for (const auto &p : paths) {
        QFileInfo fi(p);
        if (fi.isDir()) {
            QDirIterator it(p, QDir::Files | QDir::Hidden | QDir::System,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                total += it.fileInfo().size();
            }
        } else {
            total += fi.size();
        }
    }
    return total;
}
