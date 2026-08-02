#pragma once
#include <QWidget>
#include <QStringList>
#include <QPoint>

class MainWindow;

void showContextMenu(MainWindow *window, const QPoint &globalPos,
                     const QStringList &selectedPaths, const QString &currentPath);
