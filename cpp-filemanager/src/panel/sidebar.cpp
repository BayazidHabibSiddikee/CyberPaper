#include "panel/sidebar.h"
#include "app/theme.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QStorageInfo>
#include <QIcon>
#include <QStyle>
#include <QApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

static QString bookmarksFile() {
    return QDir::homePath() + "/.config/swordfm/bookmarks.json";
}

// Human-readable name for a block device, or empty if none is known.
//
// QStorageInfo::name() returns empty on this system for every volume, which
// makes displayName() fall back to the mount path — so a udisks-mounted disk
// shows up as its UUID ("F9EE-6FF0") instead of its label ("shared"). udev
// exposes the real names as symlinks, so resolve them back to the device:
// filesystem label first, then the GPT partition name.
static QString deviceLabel(const QString &device) {
    if (device.isEmpty()) return {};
    const QString target = QFileInfo(device).canonicalFilePath();
    if (target.isEmpty()) return {};

    for (const char *dir : {"/dev/disk/by-label", "/dev/disk/by-partlabel"}) {
        const QDir d(QString::fromLatin1(dir));
        if (!d.exists()) continue;
        for (const QFileInfo &link : d.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Files | QDir::Dirs)) {
            if (link.canonicalFilePath() == target)
                return link.fileName();
        }
    }
    return {};
}

SideBar::SideBar(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(180);
    setMaximumWidth(280);
    setStyleSheet(QString(
        "SideBar {"
        "  background: %1;"
        "  border-right: 1px solid %2;"
        "}"
        "QLabel {"
        "  color: %3;"
        "  font-size: 11px;"
        "  font-weight: 700;"
        "  letter-spacing: 0.5px;"
        "  padding: 10px 12px 4px 12px;"
        "}"
        "QListWidget {"
        "  background: transparent;"
        "  border: none;"
        "  outline: none;"
        "  font-size: 13px;"
        "  padding: 0 4px;"
        "  color: %4;"
        "}"
        "QListWidget::item {"
        "  padding: 6px 10px;"
        "  border-radius: 4px;"
        "  margin: 1px 4px;"
        "}"
        "QListWidget::item:hover {"
        "  background: %5;"
        "}"
        "QListWidget::item:selected {"
        "  background: %2;"
        "  color: %6;"
        "}"
    ).arg(Theme::BG2, Theme::DIM, Theme::FG_DIM, Theme::FG, Theme::HOVER, Theme::CYAN));
    buildUi();
    loadBookmarks();
    rebuildPlaces();
}

void SideBar::buildUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto makeList = [this]() {
        auto *list = new QListWidget(this);
        list->setFocusPolicy(Qt::NoFocus);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
        list->setMaximumHeight(1000);
        connect(list, &QListWidget::itemClicked, this, &SideBar::onItemClicked);
        return list;
    };

    layout->addWidget(new QLabel("PLACES", this));
    m_placesList = makeList();
    layout->addWidget(m_placesList);

    layout->addWidget(new QLabel("DEVICES", this));
    m_devicesList = makeList();
    layout->addWidget(m_devicesList);

    layout->addWidget(new QLabel("BOOKMARKS", this));
    m_bookmarksList = makeList();
    m_bookmarksList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_bookmarksList, &QListWidget::customContextMenuRequested,
            this, &SideBar::onContextMenu);
    layout->addWidget(m_bookmarksList);

    layout->addStretch(1);
}

QIcon SideBar::placeIcon(const QString &iconName) const {
    QIcon icon = QIcon::fromTheme(iconName);
    if (!icon.isNull()) return icon;

    auto *style = QApplication::style();
    if (iconName.contains("home"))
        return style->standardIcon(QStyle::SP_DirHomeIcon);
    if (iconName.contains("trash") || iconName.contains("wastebasket"))
        return style->standardIcon(QStyle::SP_TrashIcon);
    if (iconName.contains("remote") || iconName.contains("network"))
        return style->standardIcon(QStyle::SP_DriveNetIcon);
    if (iconName.contains("harddisk") || iconName.contains("drive"))
        return style->standardIcon(QStyle::SP_DriveHDIcon);
    return style->standardIcon(QStyle::SP_DirIcon);
}

void SideBar::addPlace(QListWidget *list, const QString &label, const QString &path,
                       const QString &iconName, bool /*isBookmark*/) {
    if (!path.isEmpty() && !QDir(path).exists() && path != "trash:///")
        return;
    auto *item = new QListWidgetItem(placeIcon(iconName), label);
    item->setData(Qt::UserRole, path);
    item->setToolTip(path);
    list->addItem(item);
}

void SideBar::addXdgPlace(const QString &label, QStandardPaths::StandardLocation loc,
                          const QString &iconName) {
    const QString path = QStandardPaths::writableLocation(loc);
    // An XDG dir that was never created resolves to $HOME, which would add a
    // duplicate "Home" entry under a misleading name (e.g. Music → ~).
    if (path.isEmpty() || QDir(path) == QDir(QDir::homePath()))
        return;
    addPlace(m_placesList, label, path, iconName);
}

void SideBar::rebuildPlaces() {
    m_placesList->clear();
    m_devicesList->clear();
    m_bookmarksList->clear();

    const QString home = QDir::homePath();
    addPlace(m_placesList, "Home", home, "user-home");
    addXdgPlace("Desktop",   QStandardPaths::DesktopLocation,   "user-desktop");
    addXdgPlace("Documents", QStandardPaths::DocumentsLocation, "folder-documents");
    addXdgPlace("Downloads", QStandardPaths::DownloadLocation,  "folder-download");
    addXdgPlace("Music",     QStandardPaths::MusicLocation,     "folder-music");
    addXdgPlace("Pictures",  QStandardPaths::PicturesLocation,  "folder-pictures");
    addXdgPlace("Videos",    QStandardPaths::MoviesLocation,    "folder-videos");
    addPlace(m_placesList, "Trash",
             home + "/.local/share/Trash/files", "user-trash");

    // Mounted volumes — USB/removable + rclone cloud mounts
    for (const QStorageInfo &vol : QStorageInfo::mountedVolumes()) {
        if (!vol.isValid() || !vol.isReady()) continue;
        QString root = QDir(vol.rootPath()).absolutePath();
        const QString base = QFileInfo(root).fileName();
        const QString lowerRoot = root.toLower();
        const QString lowerBase = base.toLower();
        const QString fsType = QString::fromUtf8(vol.fileSystemType()).toLower();
        const QString device = QString::fromUtf8(vol.device());
        const bool isRclone = fsType.contains("rclone")
            || (device.endsWith(':') && fsType.contains("fuse"));

        const bool isRemovablePath = root.startsWith("/run/media")
            || root.startsWith("/media")
            || root.startsWith("/mnt");

        if (!isRemovablePath && !isRclone) {
            if (root == "/" || root == "/home"
                || root.startsWith("/boot") || root.startsWith("/snap")
                || root.startsWith("/var") || root.startsWith("/proc")
                || root.startsWith("/sys") || root.startsWith("/dev")
                || root.startsWith("/run") || root.startsWith("/tmp")
                || root.startsWith("/opt") || root.startsWith("/usr")
                || root.startsWith(QDir::homePath()))
                continue;
        }

        if (lowerRoot.contains("libvirt") || lowerBase.startsWith("libvirt")
            || lowerBase.contains("libvirtd")
            || lowerBase.endsWith(".service") || lowerBase.endsWith(".socket")
            || lowerBase.endsWith(".device")
            || QRegularExpression(R"(^\d+$)").match(base).hasMatch())
            continue;

        QString name;
        if (isRclone) {
            // Prefer folder name (GoogleDrive), fall back to remote (gdrive)
            name = base;
            if (name.isEmpty()) {
                name = device;
                if (name.endsWith(':'))
                    name.chop(1);
            }
        } else {
            // Prefer the disk's own label; the mount point is often just the
            // UUID, which is meaningless to read in a sidebar.
            name = deviceLabel(device);
            if (name.isEmpty()) {
                name = vol.displayName();
                if (name.isEmpty() || name == root)
                    name = base;
            }
            if (name.isEmpty()) name = root;
        }

        if (QRegularExpression(R"(^\d+$)").match(name).hasMatch()
            || name.toLower().contains("libvirt")
            || name.toLower().endsWith(".service"))
            continue;

        QString icon = "drive-harddisk";
        if (isRclone)
            icon = "folder-remote";
        else if (isRemovablePath)
            icon = "drive-removable-media";

        addPlace(m_devicesList, name, root, icon);
        // Richer tooltip for rclone remotes
        if (isRclone && m_devicesList->count() > 0) {
            auto *item = m_devicesList->item(m_devicesList->count() - 1);
            QString remote = device;
            if (remote.endsWith(':')) remote.chop(1);
            item->setToolTip(QString("%1\nrclone: %2").arg(root, remote));
        }
    }

    for (const auto &bm : m_bookmarks) {
        QString abs = QDir(bm).absolutePath();
        if (abs == "/" || abs == "/home")
            continue;
        QString label = QDir(bm).dirName();
        if (label.isEmpty()) label = bm;
        addPlace(m_bookmarksList, label, bm, "folder", true);
    }

    // Fit height to contents. Scrollbars are disabled, so anything past the
    // fixed height is invisible rather than reachable — a hard-coded row
    // height silently swallowed the last entries once the stylesheet's
    // padding/margin pushed rows past it. sizeHintForRow() picks up the
    // stylesheet metrics without needing a layout pass to have run.
    for (auto *list : {m_placesList, m_devicesList, m_bookmarksList}) {
        int h = 8;
        if (list->count() > 0) {
            const int row = qMax(list->sizeHintForRow(0), 24);
            h = list->count() * (row + 2 * 1) + 2 * list->frameWidth() + 4;
        }
        list->setFixedHeight(qMax(h, 8));
    }

    setCurrentPath(m_currentPath);
}

void SideBar::setCurrentPath(const QString &path) {
    m_currentPath = path;
    auto highlight = [&](QListWidget *list) {
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            bool match = (item->data(Qt::UserRole).toString() == path);
            item->setSelected(match);
        }
    };
    highlight(m_placesList);
    highlight(m_devicesList);
    highlight(m_bookmarksList);
}

void SideBar::onItemClicked(QListWidgetItem *item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty())
        emit pathSelected(path);
}

void SideBar::addCurrentAsBookmark() {
    addBookmark(m_currentPath);
}

void SideBar::addBookmark(const QString &path) {
    if (path.isEmpty()) return;
    QString abs = QDir(path).absolutePath();
    QFileInfo fi(abs);
    if (!fi.exists()) return;
    // Bookmark the folder (or parent of a file)
    if (!fi.isDir())
        abs = fi.absolutePath();
    if (abs == "/" || abs == "/home") return;
    if (m_bookmarks.contains(abs)) return;
    m_bookmarks.append(abs);
    saveBookmarks();
    rebuildPlaces();
}

void SideBar::onContextMenu(const QPoint &pos) {
    auto *item = m_bookmarksList->itemAt(pos);
    QMenu menu;
    if (item) {
        menu.addAction("Remove Bookmark", this, [this, item]() {
            QString path = item->data(Qt::UserRole).toString();
            m_bookmarks.removeOne(path);
            saveBookmarks();
            rebuildPlaces();
        });
    }
    menu.addAction("Add Current Folder", this, &SideBar::addCurrentAsBookmark);
    menu.exec(m_bookmarksList->mapToGlobal(pos));
}

void SideBar::loadBookmarks() {
    QFile f(bookmarksFile());
    if (!f.open(QIODevice::ReadOnly))
        return;
    auto doc = QJsonDocument::fromJson(f.readAll());
    for (const auto &v : doc["bookmarks"].toArray())
        m_bookmarks.append(v.toString());
}

void SideBar::saveBookmarks() {
    QDir().mkpath(QFileInfo(bookmarksFile()).absolutePath());
    QJsonArray arr;
    for (const auto &bm : m_bookmarks)
        arr.append(bm);
    QJsonObject obj;
    obj["bookmarks"] = arr;
    QFile f(bookmarksFile());
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}
