#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QSet>

#include <atomic>

// Recursive filesystem scanner running off the GUI thread.
//
// Uses POSIX readdir() rather than QDirIterator for two reasons: dirent's
// d_type tells us file-vs-directory without a stat() syscall, and we can then
// stat() only the entries that already matched the extension filter. On a home
// directory with tens of thousands of files that turns nearly every stat into
// a skipped one.
//
// Directories are handed to a small pool of worker threads, so a scan
// saturates an SSD's queue depth instead of walking one directory at a time.
class FileScanner : public QObject {
    Q_OBJECT
public:
    struct Hit {
        QString path;
        qint64 size = 0;
        qint64 mtime = 0;   // seconds since epoch
    };

    // suffixes are lowercase and without the dot; empty means "any type".
    // fromEpoch/toEpoch are inclusive day bounds, -1 when unset.
    FileScanner(QString root, QSet<QString> suffixes,
                qint64 fromEpoch, qint64 toEpoch, int maxResults,
                bool includeHidden = false, QObject *parent = nullptr);

    // Safe to call from any thread.
    void cancel() { m_stop.store(true); }

public slots:
    void run();

signals:
    void batch(QVector<FileScanner::Hit> hits);
    void finished(int total, bool truncated);

private:
    QString m_root;
    QSet<QString> m_suffixes;
    qint64 m_from;
    qint64 m_to;
    int m_max;
    bool m_hidden;
    std::atomic<bool> m_stop{false};
};

Q_DECLARE_METATYPE(FileScanner::Hit)
Q_DECLARE_METATYPE(QVector<FileScanner::Hit>)
