#pragma once
#include <QStandardItemModel>
#include <QStringList>
#include <QDate>
#include <QVector>

#include "model/filefilter.h"
#include "model/filescanner.h"

class QThread;

// Flat, recursive result set for type/date searches. QFileSystemModel only
// ever exposes one directory at a time, so matching "every image under here"
// needs its own model.
//
// The walk runs on a worker thread (see FileScanner) and results stream in as
// batches, so a scan of $HOME shows its first hits immediately instead of
// freezing the window until the whole tree has been read.
class SearchModel : public QStandardItemModel {
    Q_OBJECT
public:
    explicit SearchModel(QObject *parent = nullptr);
    ~SearchModel() override;

    // Clears the model and starts a background walk of `root`. Any scan
    // already running is abandoned. Progress arrives via progress()/completed().
    void startSearch(const QString &root, FileFilterProxy::TypeFilter type,
                     const QDate &from, const QDate &to,
                     bool includeHidden = false, int maxResults = 20000);
    void stopSearch();

    QString pathAt(const QModelIndex &index) const;
    bool truncated() const { return m_truncated; }
    bool running() const { return m_thread != nullptr; }

signals:
    void progress(int found);
    void completed(int found, bool truncated);

private:
    void appendBatch(const QVector<FileScanner::Hit> &hits);
    void onFinished(int total, bool truncated);

    QThread *m_thread = nullptr;
    FileScanner *m_scanner = nullptr;
    QString m_root;
    int m_found = 0;
    bool m_truncated = false;
};
