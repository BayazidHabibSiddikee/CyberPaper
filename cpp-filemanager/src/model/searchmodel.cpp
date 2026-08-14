#include "model/searchmodel.h"

#include <QDateTime>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QThread>

namespace {

QString humanSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 bytes").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KiB").arg(bytes / 1024.0, 0, 'f', 2);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MiB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
    return QString("%1 GiB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

// Icon and type-name lookup goes through the MIME database and the icon theme,
// which is far too slow to repeat for every one of twenty thousand results.
// Files sharing an extension always resolve identically, so one lookup per
// extension is enough.
struct IconCache {
    QFileIconProvider provider;
    QHash<QString, QIcon> icons;
    QHash<QString, QString> types;

    void lookup(const QFileInfo &fi, QIcon *icon, QString *type) {
        const QString key = fi.suffix().toLower();
        auto it = icons.constFind(key);
        if (it != icons.constEnd()) {
            *icon = *it;
            *type = types.value(key);
            return;
        }
        *icon = provider.icon(fi);
        *type = provider.type(fi);
        icons.insert(key, *icon);
        types.insert(key, *type);
    }
};

} // namespace

SearchModel::SearchModel(QObject *parent)
    : QStandardItemModel(parent)
{
    qRegisterMetaType<QVector<FileScanner::Hit>>("QVector<FileScanner::Hit>");
    // Location is its own column because results come from many folders; folding
    // the folder into Name would push the filename out of the visible width.
    setHorizontalHeaderLabels({"Name", "Location", "Size", "Type", "Date Modified"});
}

SearchModel::~SearchModel() {
    stopSearch();
}

QString SearchModel::pathAt(const QModelIndex &index) const {
    if (!index.isValid())
        return {};
    QStandardItem *it = item(index.row(), 0);
    return it ? it->data(Qt::UserRole + 1).toString() : QString();
}

void SearchModel::stopSearch() {
    if (!m_thread)
        return;
    m_scanner->cancel();
    m_thread->quit();
    m_thread->wait();
    // The thread owns the scanner via deleteLater, but that will not run now
    // that the loop has exited, so drop both here.
    delete m_scanner;
    delete m_thread;
    m_scanner = nullptr;
    m_thread = nullptr;
}

void SearchModel::startSearch(const QString &root, FileFilterProxy::TypeFilter type,
                              const QDate &from, const QDate &to,
                              bool includeHidden, int maxResults)
{
    stopSearch();

    removeRows(0, rowCount());
    m_truncated = false;
    m_found = 0;
    m_root = root;

    const bool anyType = (type == FileFilterProxy::AnyType);
    if (anyType && !from.isValid() && !to.isValid()) {
        emit completed(0, false);
        return;
    }

    QSet<QString> suffixes;
    if (!anyType) {
        const QStringList &wanted = FileFilterProxy::suffixesFor(type);
        for (const QString &s : wanted)
            suffixes.insert(s.toLower());
    }

    // Inclusive day bounds: `to` must cover the whole of that day, so take the
    // start of the following one minus a second.
    const qint64 fromEpoch = from.isValid()
        ? QDateTime(from, QTime(0, 0), QTimeZone::LocalTime).toSecsSinceEpoch() : -1;
    const qint64 toEpoch = to.isValid()
        ? QDateTime(to, QTime(23, 59, 59), QTimeZone::LocalTime).toSecsSinceEpoch() : -1;

    m_thread = new QThread;
    m_scanner = new FileScanner(root, suffixes, fromEpoch, toEpoch, maxResults, includeHidden);
    m_scanner->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_scanner, &FileScanner::run);
    connect(m_scanner, &FileScanner::batch, this, &SearchModel::appendBatch);
    connect(m_scanner, &FileScanner::finished, this, &SearchModel::onFinished);
    m_thread->start();
}

void SearchModel::appendBatch(const QVector<FileScanner::Hit> &hits) {
    static IconCache cache;
    const QDir rootDir(m_root);

    for (const FileScanner::Hit &h : hits) {
        const QFileInfo fi(h.path);

        QIcon icon;
        QString typeName;
        cache.lookup(fi, &icon, &typeName);

        auto *nameItem = new QStandardItem(icon, fi.fileName());
        nameItem->setData(h.path, Qt::UserRole + 1);
        nameItem->setToolTip(h.path);
        nameItem->setEditable(false);

        QString dir = rootDir.relativeFilePath(fi.absolutePath());
        if (dir.isEmpty() || dir == ".")
            dir = QStringLiteral("./");
        auto *dirItem = new QStandardItem(dir);
        dirItem->setToolTip(fi.absolutePath());
        dirItem->setEditable(false);

        auto *sizeItem = new QStandardItem(humanSize(h.size));
        sizeItem->setEditable(false);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto *typeItem = new QStandardItem(typeName);
        typeItem->setEditable(false);

        auto *dateItem = new QStandardItem(QLocale().toString(
            QDateTime::fromSecsSinceEpoch(h.mtime), "M/d/yy h:mm AP"));
        dateItem->setEditable(false);

        appendRow({nameItem, dirItem, sizeItem, typeItem, dateItem});
    }

    m_found += hits.size();
    emit progress(m_found);
}

void SearchModel::onFinished(int total, bool truncated) {
    m_found = total;
    m_truncated = truncated;
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_scanner;
        delete m_thread;
        m_scanner = nullptr;
        m_thread = nullptr;
    }
    emit completed(total, truncated);
}
