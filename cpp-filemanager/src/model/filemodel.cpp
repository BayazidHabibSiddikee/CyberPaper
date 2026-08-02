#include "model/filemodel.h"
#include <QIcon>
#include <QFileInfo>
#include <QFont>

FileModel::FileModel(QObject *parent)
    : QFileSystemModel(parent) {}

QVariant FileModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();

    if (role == Qt::FontRole) {
        QFont font;
        font.setPointSize(10);
        return font;
    }

    return QFileSystemModel::data(index, role);
}

QVariant FileModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole) {
        switch (section) {
            case 0: return "Name";
            case 1: return "Size";
            case 2: return "Type";
            case 3: return "Date Modified";
        }
    }
    return QFileSystemModel::headerData(section, orientation, role);
}
