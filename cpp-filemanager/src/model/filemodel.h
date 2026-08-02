#pragma once
#include <QFileSystemModel>
#include <QModelIndex>

class FileModel : public QFileSystemModel {
    Q_OBJECT
public:
    explicit FileModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
};
