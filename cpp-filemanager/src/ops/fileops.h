#pragma once
#include <QString>
#include <QStringList>

bool copyFiles(const QStringList &srcPaths, const QString &destDir);
bool moveFiles(const QStringList &srcPaths, const QString &destDir);
bool deleteFileOrDir(const QString &path);
qint64 selectedTotalSize(const QStringList &paths);
QString uniqueDestPath(const QString &destDir, const QString &fileName);
