#include "model/searchmodel.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QFileIconProvider>
#include <QDir>
#include <QLocale>

namespace {

QString humanSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 bytes").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KiB").arg(bytes / 1024.0, 0, 'f', 2);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MiB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
    return QString("%1 GiB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

} // namespace

SearchModel::SearchModel(QObject *parent)
    : QStandardItemModel(parent)
{
    // Location is its own column because results come from many folders; folding
    // the folder into Name would push the filename out of the visible width.
    setHorizontalHeaderLabels({"Name", "Location", "Size", "Type", "Date Modified"});
}

QString SearchModel::pathAt(const QModelIndex &index) const {
    if (!index.isValid())
        return {};
    QStandardItem *it = item(index.row(), 0);
    return it ? it->data(Qt::UserRole + 1).toString() : QString();
}

int SearchModel::search(const QString &root, FileFilterProxy::TypeFilter type,
                        const QDate &from, const QDate &to, int maxResults)
{
    removeRows(0, rowCount());
    m_truncated = false;

    const QStringList &wanted = FileFilterProxy::suffixesFor(type);
    const bool anyType = (type == FileFilterProxy::AnyType);
    if (anyType && !from.isValid() && !to.isValid())
        return 0;

    QFileIconProvider icons;
    const QDir rootDir(root);
    int found = 0;

    QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();

        if (!anyType && !wanted.contains(fi.suffix().toLower()))
            continue;

        if (from.isValid() || to.isValid()) {
            const QDate mod = fi.lastModified().date();
            if (from.isValid() && mod < from) continue;
            if (to.isValid() && mod > to) continue;
        }

        auto *nameItem = new QStandardItem(icons.icon(fi), fi.fileName());
        nameItem->setData(fi.absoluteFilePath(), Qt::UserRole + 1);
        nameItem->setToolTip(fi.absoluteFilePath());
        nameItem->setEditable(false);

        QString dir = rootDir.relativeFilePath(fi.absolutePath());
        if (dir.isEmpty() || dir == ".")
            dir = QStringLiteral("./");
        auto *dirItem = new QStandardItem(dir);
        dirItem->setToolTip(fi.absolutePath());
        dirItem->setEditable(false);

        auto *sizeItem = new QStandardItem(humanSize(fi.size()));
        sizeItem->setEditable(false);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto *typeItem = new QStandardItem(icons.type(fi));
        typeItem->setEditable(false);

        auto *dateItem = new QStandardItem(
            QLocale().toString(fi.lastModified(), "M/d/yy h:mm AP"));
        dateItem->setEditable(false);

        appendRow({nameItem, dirItem, sizeItem, typeItem, dateItem});

        if (++found >= maxResults) {
            m_truncated = true;
            break;
        }
    }

    return found;
}
