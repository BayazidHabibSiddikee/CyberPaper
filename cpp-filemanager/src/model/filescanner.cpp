#include "model/filescanner.h"

#include <QElapsedTimer>
#include <QThread>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

namespace {

// Lowercase extension after the final dot, empty if there isn't one in the
// filename itself (a dot in a parent directory name is not our concern since
// we only ever look at d_name).
QString suffixOf(const char *name, int len) {
    int dot = -1;
    for (int i = len - 1; i >= 0; --i) {
        if (name[i] == '.') { dot = i; break; }
    }
    if (dot < 0 || dot == len - 1)
        return {};
    return QString::fromUtf8(name + dot + 1, len - dot - 1).toLower();
}

// Shared work queue. Workers both consume directories and produce their
// subdirectories, so `active` tracks in-flight work: the scan is only done when
// the queue is empty *and* nobody is still walking a directory.
struct Queue {
    std::mutex m;
    std::condition_variable cv;
    std::deque<std::string> dirs;
    int active = 0;
    bool done = false;

    void push(std::string d) {
        {
            std::lock_guard<std::mutex> lk(m);
            dirs.push_back(std::move(d));
        }
        cv.notify_one();
    }

    bool pop(std::string &out) {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return !dirs.empty() || done; });
        if (dirs.empty())
            return false;
        out = std::move(dirs.front());
        dirs.pop_front();
        ++active;
        return true;
    }

    void finishOne() {
        std::lock_guard<std::mutex> lk(m);
        if (--active == 0 && dirs.empty()) {
            done = true;
            cv.notify_all();
        }
    }

    void abort() {
        std::lock_guard<std::mutex> lk(m);
        done = true;
        dirs.clear();
        cv.notify_all();
    }
};

} // namespace

FileScanner::FileScanner(QString root, QSet<QString> suffixes,
                         qint64 fromEpoch, qint64 toEpoch, int maxResults,
                         bool includeHidden, QObject *parent)
    : QObject(parent), m_root(std::move(root)), m_suffixes(std::move(suffixes)),
      m_from(fromEpoch), m_to(toEpoch), m_max(maxResults),
      m_hidden(includeHidden) {}

void FileScanner::run() {
    const bool anyType = m_suffixes.isEmpty();
    const bool dateFiltered = (m_from >= 0 || m_to >= 0);

    // Stay on one filesystem, the way `find -xdev` does. A home directory here
    // typically has several rclone/FUSE cloud mounts under it, and descending
    // into one turns every readdir into a network round trip — that alone can
    // make a search of $HOME take minutes. Starting the search *inside* a mount
    // still works, because the root's device becomes the reference.
    dev_t rootDev = 0;
    {
        struct stat st;
        if (stat(m_root.toUtf8().constData(), &st) != 0) {
            emit finished(0, false);
            return;
        }
        rootDev = st.st_dev;
    }

    Queue queue;
    queue.push(m_root.toStdString());

    std::mutex outMutex;
    QVector<Hit> pending;
    int total = 0;
    bool truncated = false;

    // Emit in batches rather than per-hit: a signal per file would flood the
    // event loop and make the UI slower than the synchronous version was.
    QElapsedTimer clock;
    clock.start();
    qint64 lastFlush = 0;

    auto flush = [&](bool force) {
        QVector<Hit> out;
        {
            std::lock_guard<std::mutex> lk(outMutex);
            if (pending.isEmpty())
                return;
            const qint64 now = clock.elapsed();
            if (!force && pending.size() < 200 && now - lastFlush < 100)
                return;
            lastFlush = now;
            out.swap(pending);
        }
        emit batch(out);
    };

    unsigned hw = std::thread::hardware_concurrency();
    const int workers = std::max(2u, std::min(8u, hw ? hw : 4u));

    std::vector<std::thread> pool;
    for (int i = 0; i < workers; ++i) {
        pool.emplace_back([&] {
            std::string dir;
            while (queue.pop(dir)) {
                if (m_stop.load()) { queue.finishOne(); continue; }

                DIR *d = opendir(dir.c_str());
                if (!d) { queue.finishOne(); continue; }   // unreadable: skip silently

                // One fstat per directory (not per entry) rejects mount points.
                {
                    struct stat dst;
                    if (fstat(dirfd(d), &dst) != 0 || dst.st_dev != rootDev) {
                        closedir(d);
                        queue.finishOne();
                        continue;
                    }
                }

                QVector<Hit> local;
                struct dirent *e;
                while ((e = readdir(d)) != nullptr) {
                    const char *n = e->d_name;
                    if (n[0] == '.' &&
                        (n[1] == '\0' || (n[1] == '.' && n[2] == '\0')))
                        continue;
                    if (!m_hidden && n[0] == '.')
                        continue;

                    std::string full = dir;
                    if (full.back() != '/') full += '/';
                    full += n;

                    unsigned char type = e->d_type;
                    // Some filesystems (older XFS, some network mounts) report
                    // DT_UNKNOWN; only then do we pay for a stat().
                    if (type == DT_UNKNOWN) {
                        struct stat st;
                        if (lstat(full.c_str(), &st) != 0) continue;
                        if (S_ISDIR(st.st_mode)) type = DT_DIR;
                        else if (S_ISREG(st.st_mode)) type = DT_REG;
                        else continue;
                    }

                    if (type == DT_DIR) {
                        queue.push(std::move(full));
                        continue;
                    }
                    if (type != DT_REG)
                        continue;

                    // Extension test first: it is pure string work, and it
                    // rejects the overwhelming majority of entries before we
                    // ever touch the disk for metadata.
                    if (!anyType) {
                        const int len = int(strlen(n));
                        if (!m_suffixes.contains(suffixOf(n, len)))
                            continue;
                    }

                    struct stat st;
                    if (stat(full.c_str(), &st) != 0)
                        continue;

                    if (dateFiltered) {
                        if (m_from >= 0 && st.st_mtime < m_from) continue;
                        if (m_to   >= 0 && st.st_mtime > m_to)   continue;
                    }

                    Hit h;
                    h.path = QString::fromStdString(full);
                    h.size = st.st_size;
                    h.mtime = st.st_mtime;
                    local.push_back(std::move(h));
                }
                closedir(d);

                if (!local.isEmpty()) {
                    std::lock_guard<std::mutex> lk(outMutex);
                    if (total < m_max) {
                        for (Hit &h : local) {
                            if (total >= m_max) { truncated = true; break; }
                            pending.push_back(std::move(h));
                            ++total;
                        }
                    } else {
                        truncated = true;
                    }
                    if (truncated)
                        m_stop.store(true);
                }

                queue.finishOne();
                if (m_stop.load())
                    queue.abort();
            }
        });
    }

    // Pump batches to the GUI while the workers run.
    while (true) {
        {
            std::unique_lock<std::mutex> lk(queue.m);
            if (queue.done)
                break;
        }
        QThread::msleep(40);
        flush(false);
    }

    for (auto &t : pool)
        t.join();

    flush(true);
    emit finished(total, truncated);
}
