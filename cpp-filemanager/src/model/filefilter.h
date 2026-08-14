#pragma once
#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QDate>
#include <QSet>

class FileFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    enum TypeFilter { AnyType, Images, Videos, Audio, Documents, Archives, Discs };

    explicit FileFilterProxy(QObject *parent = nullptr);

    void setSourceFsModel(QFileSystemModel *model);
    QFileSystemModel *fsModel() const;

    // Directories are never hidden by these filters, otherwise the filtered
    // view would have no way to navigate anywhere.
    void setTypeFilter(TypeFilter type);
    void setDateRange(const QDate &from, const QDate &to);
    void clearDateRange();

    // Marks are a sticky multi-selection that survives navigating between
    // folders, so a copy/move/delete can gather items from several places.
    void toggleMark(const QString &path);
    void setMarked(const QString &path, bool on);
    void clearMarks();
    bool isMarked(const QString &path) const { return m_marked.contains(path); }
    QStringList markedPaths() const;
    int markCount() const { return m_marked.size(); }

    QString filePath(const QModelIndex &proxyIndex) const;
    QFileInfo fileInfo(const QModelIndex &proxyIndex) const;
    bool isDir(const QModelIndex &proxyIndex) const;

    static const QStringList &suffixesFor(TypeFilter type);

signals:
    void marksChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    QVariant data(const QModelIndex &proxyIndex, int role) const override;
    bool setData(const QModelIndex &proxyIndex, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &proxyIndex) const override;

private:
    static bool isJunkName(const QString &name);
    void emitRowChanged(const QString &path);

    TypeFilter m_type = AnyType;
    QDate m_from;
    QDate m_to;
    QSet<QString> m_marked;
};
