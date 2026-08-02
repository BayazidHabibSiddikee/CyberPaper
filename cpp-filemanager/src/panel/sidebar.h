#pragma once
#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QStringList>
#include <QLabel>
#include <QIcon>
#include <QStandardPaths>

class SideBar : public QWidget {
    Q_OBJECT
public:
    explicit SideBar(QWidget *parent = nullptr);

    void setCurrentPath(const QString &path);
    void addCurrentAsBookmark();
    void addBookmark(const QString &path);

signals:
    void pathSelected(const QString &path);

private slots:
    void onItemClicked(QListWidgetItem *item);
    void onContextMenu(const QPoint &pos);

private:
    void buildUi();
    void rebuildPlaces();
    void loadBookmarks();
    void saveBookmarks();
    void addPlace(QListWidget *list, const QString &label, const QString &path,
                  const QString &iconName, bool isBookmark = false);
    void addXdgPlace(const QString &label, QStandardPaths::StandardLocation loc,
                     const QString &iconName);
    QIcon placeIcon(const QString &iconName) const;

    QListWidget *m_placesList;
    QListWidget *m_devicesList;
    QListWidget *m_bookmarksList;
    QStringList m_bookmarks;
    QString m_currentPath;
};
