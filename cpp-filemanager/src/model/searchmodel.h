#pragma once
#include <QStandardItemModel>
#include <QStringList>
#include <QDate>

#include "model/filefilter.h"

// Flat, recursive result set for type/date searches. QFileSystemModel only
// ever exposes one directory at a time, so matching "every image under here"
// needs its own model; the Name column carries the path relative to the search
// root so results from different folders stay distinguishable.
class SearchModel : public QStandardItemModel {
    Q_OBJECT
public:
    explicit SearchModel(QObject *parent = nullptr);

    // Walks `root` recursively. Returns the number of matches, capped at
    // maxResults so a scan of $HOME cannot lock the UI indefinitely.
    int search(const QString &root, FileFilterProxy::TypeFilter type,
               const QDate &from, const QDate &to, int maxResults = 5000);

    QString pathAt(const QModelIndex &index) const;
    bool truncated() const { return m_truncated; }

private:
    bool m_truncated = false;
};
