#include "model/filefilter.h"
#include "app/theme.h"

#include <QFileInfo>
#include <QDateTime>
#include <QColor>
#include <QRegularExpression>

FileFilterProxy::FileFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void FileFilterProxy::setSourceFsModel(QFileSystemModel *model) {
    setSourceModel(model);
}

QFileSystemModel *FileFilterProxy::fsModel() const {
    return qobject_cast<QFileSystemModel*>(sourceModel());
}

QString FileFilterProxy::filePath(const QModelIndex &proxyIndex) const {
    auto *fs = fsModel();
    if (!fs || !proxyIndex.isValid()) return {};
    return fs->filePath(mapToSource(proxyIndex));
}

QFileInfo FileFilterProxy::fileInfo(const QModelIndex &proxyIndex) const {
    auto *fs = fsModel();
    if (!fs || !proxyIndex.isValid()) return {};
    return fs->fileInfo(mapToSource(proxyIndex));
}

bool FileFilterProxy::isDir(const QModelIndex &proxyIndex) const {
    auto *fs = fsModel();
    if (!fs || !proxyIndex.isValid()) return false;
    return fs->isDir(mapToSource(proxyIndex));
}

bool FileFilterProxy::isJunkName(const QString &name) {
    if (name.isEmpty()) return true;

    // Pure numeric names (e.g. "1000" from /run/user/1000 clutter)
    static const QRegularExpression pureNum(R"(^\d+$)");
    if (pureNum.match(name).hasMatch())
        return true;

    const QString lower = name.toLower();

    // libvirt / virt clutter
    if (lower.startsWith("libvirt") || lower.startsWith("libvirtd")
        || lower.contains("libvirt"))
        return true;

    // systemd unit / service files
    if (lower.endsWith(".service") || lower.endsWith(".socket")
        || lower.endsWith(".target") || lower.endsWith(".timer")
        || lower.endsWith(".mount") || lower.endsWith(".scope")
        || lower.endsWith(".slice") || lower.endsWith(".path")
        || lower.endsWith(".device") || lower.endsWith(".automount")
        || lower.endsWith(".swap"))
        return true;

    return false;
}

const QStringList &FileFilterProxy::suffixesFor(TypeFilter type) {
    static const QStringList none;
    static const QStringList images = {
        "png","jpg","jpeg","gif","webp","bmp","svg","ico","tif","tiff",
        "avif","jxl","heic","heif","raw","cr2","nef","arw","dng","psd","xcf",
    };
    static const QStringList videos = {
        "mp4","mkv","avi","mov","wmv","flv","webm","m4v","mpg","mpeg",
        "3gp","ogv","ts","m2ts","vob","rmvb","divx",
    };
    static const QStringList audio = {
        "mp3","flac","wav","ogg","opus","m4a","aac","wma","aiff","ape",
        "alac","mid","midi",
    };
    static const QStringList documents = {
        "pdf","doc","docx","odt","rtf","txt","md","xls","xlsx","ods","csv",
        "ppt","pptx","odp","epub","mobi","djvu","tex",
    };
    static const QStringList archives = {
        "zip","rar","7z","tar","gz","bz2","xz","zst","lz","lzma","tgz",
        "tbz2","txz","cab","arj","lha","deb","rpm","pkg","apk","jar",
    };
    static const QStringList discs = {
        "iso","img","dmg","vdi","vmdk","qcow2","cue","bin","mdf","nrg","toast",
    };

    switch (type) {
    case Images:    return images;
    case Videos:    return videos;
    case Audio:     return audio;
    case Documents: return documents;
    case Archives:  return archives;
    case Discs:     return discs;
    case AnyType:   break;
    }
    return none;
}

void FileFilterProxy::setTypeFilter(TypeFilter type) {
    if (m_type == type)
        return;
    m_type = type;
    beginFilterChange();
    endFilterChange();
}

void FileFilterProxy::setDateRange(const QDate &from, const QDate &to) {
    if (m_from == from && m_to == to)
        return;
    m_from = from;
    m_to = to;
    beginFilterChange();
    endFilterChange();
}

void FileFilterProxy::clearDateRange() {
    setDateRange(QDate(), QDate());
}

void FileFilterProxy::emitRowChanged(const QString &path) {
    auto *fs = fsModel();
    if (!fs) return;
    QModelIndex prox = mapFromSource(fs->index(path));
    if (prox.isValid())
        emit dataChanged(prox, prox, {Qt::CheckStateRole, Qt::ForegroundRole});
}

void FileFilterProxy::setMarked(const QString &path, bool on) {
    if (path.isEmpty()) return;
    const bool had = m_marked.contains(path);
    if (had == on) return;
    if (on) m_marked.insert(path);
    else    m_marked.remove(path);
    emitRowChanged(path);
    emit marksChanged();
}

void FileFilterProxy::toggleMark(const QString &path) {
    setMarked(path, !m_marked.contains(path));
}

void FileFilterProxy::clearMarks() {
    if (m_marked.isEmpty()) return;
    const QStringList old = m_marked.values();
    m_marked.clear();
    for (const QString &p : old)
        emitRowChanged(p);
    emit marksChanged();
}

QStringList FileFilterProxy::markedPaths() const {
    QStringList out = m_marked.values();
    out.sort(Qt::CaseInsensitive);
    return out;
}

QVariant FileFilterProxy::data(const QModelIndex &proxyIndex, int role) const {
    if (proxyIndex.column() == 0) {
        if (role == Qt::CheckStateRole)
            return m_marked.contains(filePath(proxyIndex)) ? Qt::Checked : Qt::Unchecked;
        if (role == Qt::ForegroundRole && m_marked.contains(filePath(proxyIndex)))
            return QColor(Theme::AMBER);
    }
    return QSortFilterProxyModel::data(proxyIndex, role);
}

bool FileFilterProxy::setData(const QModelIndex &proxyIndex, const QVariant &value, int role) {
    if (role == Qt::CheckStateRole && proxyIndex.column() == 0) {
        setMarked(filePath(proxyIndex), value.toInt() == Qt::Checked);
        return true;
    }
    return QSortFilterProxyModel::setData(proxyIndex, value, role);
}

Qt::ItemFlags FileFilterProxy::flags(const QModelIndex &proxyIndex) const {
    Qt::ItemFlags f = QSortFilterProxyModel::flags(proxyIndex);
    if (proxyIndex.column() == 0)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

bool FileFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    auto *fs = fsModel();
    if (!fs) return true;

    QModelIndex idx = fs->index(sourceRow, 0, sourceParent);
    if (!idx.isValid()) return false;

    const QString name = fs->fileName(idx);
    if (isJunkName(name))
        return false;

    // Folders always stay visible — filtering them out would strand the user
    // in a directory they cannot navigate out of.
    if (fs->isDir(idx))
        return true;

    const QFileInfo fi = fs->fileInfo(idx);

    if (m_type != AnyType && !suffixesFor(m_type).contains(fi.suffix().toLower()))
        return false;

    if (m_from.isValid() || m_to.isValid()) {
        const QDate mod = fi.lastModified().date();
        if (m_from.isValid() && mod < m_from)
            return false;
        if (m_to.isValid() && mod > m_to)
            return false;
    }

    return true;
}
