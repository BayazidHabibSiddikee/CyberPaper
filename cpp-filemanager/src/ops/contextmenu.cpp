#include "ops/contextmenu.h"
#include "app/mainwindow.h"
#include "ops/termutil.h"
#include "app/theme.h"
#include "ops/openwith.h"
#include "ops/archiveops.h"

#include <QMenu>
#include <QAction>
#include <QFileInfo>
#include <QMessageBox>
#include <QIcon>
#include <QDateTime>
#include <QMimeDatabase>

void showContextMenu(MainWindow *window, const QPoint &globalPos,
                     const QStringList &selectedPaths, const QString &currentPath)
{
    Q_UNUSED(currentPath);

    QMenu menu(window);
    menu.setStyleSheet(QString(
        "QMenu { background: %1; color: %2; border: 1px solid %3; }"
        "QMenu::item { padding: 6px 24px 6px 16px; }"
        "QMenu::item:selected { background: %3; color: %4; }"
        "QMenu::separator { height: 1px; background: %3; margin: 4px 8px; }"
    ).arg(Theme::BG2, Theme::FG, Theme::DIM, Theme::CYAN));

    if (selectedPaths.isEmpty()) {
        menu.addAction(QIcon::fromTheme("folder-new"), "New Folder",
                       window, &MainWindow::createNewFolder);
        menu.addAction(QIcon::fromTheme("document-new"), "New File",
                       window, &MainWindow::createNewFile);
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("edit-paste"), "Paste",
                       window, &MainWindow::pasteClipboard);
        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("view-refresh"), "Refresh",
                       window, &MainWindow::refresh);
        menu.addAction(QIcon::fromTheme("office-chart-scatter"), "Open in SwordGraph",
                       window, &MainWindow::graphCurrent);
        menu.addAction(QIcon::fromTheme("utilities-terminal"), "Open Terminal Here",
                       window, &MainWindow::openTerminalHere);
        menu.addAction(QIcon::fromTheme("bookmark-new"), "Bookmark This Folder",
                       window, &MainWindow::bookmarkCurrent);
    } else {
        const QString path = selectedPaths.first();
        QFileInfo fi(path);

        // Marks override the selection for cut/copy/delete, so label those
        // actions with the count the user will actually act on.
        const int markCount = window->markCount();
        const bool allMarked = window->allMarked(selectedPaths);
        const int actCount = markCount > 0 ? markCount : selectedPaths.size();
        const QString suffix = actCount > 1 ? QString(" %1 items").arg(actCount) : QString();
        const QString cutLabel    = "Cut" + suffix;
        const QString copyLabel   = "Copy" + suffix;
        const QString deleteLabel = "Delete" + suffix;

        menu.addAction(QIcon::fromTheme("document-open"), "Open", window, [window, selectedPaths]() {
            for (const auto &p : selectedPaths)
                window->openFile(p);
        });

        if (selectedPaths.size() == 1 && fi.isFile()) {
            const auto handlers = appsForFile(path);
            if (!handlers.isEmpty()) {
                auto *openWith = menu.addMenu(QIcon::fromTheme("document-open"), "Open With");
                for (const AppHandler &app : handlers) {
                    QString label = app.name;
                    if (app.isDefault)
                        label += " (default)";
                    QAction *act = openWith->addAction(app.icon(), label);
                    AppHandler launched = app;
                    QObject::connect(act, &QAction::triggered, window, [launched, path, window]() {
                        if (!openWithApp(launched, path)) {
                            QMessageBox::warning(window, "SwordFM",
                                                 QString("Could not launch %1").arg(launched.name));
                        }
                    });
                }
            }
        }

        menu.addAction(QIcon::fromTheme("document-preview"), "Preview",
                       window, &MainWindow::previewSelected);
        if (selectedPaths.size() == 1 && fi.isDir()) {
            menu.addAction(QIcon::fromTheme("office-chart-scatter"), "Open in SwordGraph",
                           window, &MainWindow::graphSelected);
        }
        menu.addAction(QIcon::fromTheme("utilities-terminal"), "Open Terminal Here",
                       window, &MainWindow::openTerminalHere);
        menu.addAction(QIcon::fromTheme("bookmark-new"), "Add Bookmark",
                       window, &MainWindow::bookmarkSelection);

        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("edit-select-all"),
                       allMarked ? "Unmark" : "Mark  (Space)",
                       window, &MainWindow::toggleMarkSelection);
        if (markCount > 0) {
            menu.addAction(QIcon::fromTheme("edit-clear"),
                           QString("Clear %1 mark%2").arg(markCount)
                               .arg(markCount == 1 ? "" : "s"),
                           window, &MainWindow::clearMarks);
        }

        menu.addSeparator();

        // Archive actions. Extract only appears when every selected item is an
        // archive, so it never shows up as a no-op on a mixed selection.
        bool allArchives = true;
        for (const QString &p : selectedPaths) {
            if (!isArchive(p)) { allArchives = false; break; }
        }
        if (allArchives) {
            menu.addAction(QIcon::fromTheme("archive-extract"), "Extract Here",
                           window, &MainWindow::extractSelectionHere);
            menu.addAction(QIcon::fromTheme("archive-extract"), "Extract to Subfolder",
                           window, &MainWindow::extractSelectionToFolder);
        }

        const auto formats = availableFormats();
        if (!formats.isEmpty()) {
            auto *compress = menu.addMenu(QIcon::fromTheme("package-x-generic"),
                                          "Compress");
            for (const ArchiveFormat &f : formats) {
                QAction *act = compress->addAction(f.label);
                const QString id = f.id;
                QObject::connect(act, &QAction::triggered, window,
                                 [window, id]() { window->compressSelection(id); });
            }
        }

        menu.addSeparator();
        menu.addAction(QIcon::fromTheme("edit-cut"), cutLabel,
                       window, &MainWindow::cutSelection);
        menu.addAction(QIcon::fromTheme("edit-copy"), copyLabel,
                       window, &MainWindow::copySelection);
        menu.addAction(QIcon::fromTheme("edit-paste"), "Paste",
                       window, &MainWindow::pasteClipboard);

        menu.addSeparator();
        if (selectedPaths.size() == 1) {
            menu.addAction(QIcon::fromTheme("edit-rename"), "Rename",
                           window, &MainWindow::renameSelected);
        }
        menu.addAction(QIcon::fromTheme("edit-delete"), deleteLabel,
                       window, &MainWindow::deleteSelection);

        if (selectedPaths.size() == 1) {
            menu.addSeparator();
            menu.addAction(QIcon::fromTheme("document-properties"), "Properties",
                           window, [window, path]() {
                QFileInfo info(path);
                QString perms;
                perms += info.isReadable() ? 'r' : '-';
                perms += info.isWritable() ? 'w' : '-';
                perms += info.isExecutable() ? 'x' : '-';

                QString sizeStr;
                if (info.isDir()) {
                    sizeStr = "Directory";
                } else {
                    qint64 s = info.size();
                    if (s < 1024) sizeStr = QString("%1 bytes").arg(s);
                    else if (s < 1024 * 1024) sizeStr = QString("%1 KB").arg(s / 1024.0, 0, 'f', 1);
                    else sizeStr = QString("%1 MB").arg(s / (1024.0 * 1024), 0, 'f', 1);
                }

                QMimeDatabase mdb;
                QString text = QString(
                    "<b>%1</b><br><br>"
                    "<b>Location:</b> %2<br>"
                    "<b>Size:</b> %3<br>"
                    "<b>Type:</b> %4<br>"
                    "<b>Modified:</b> %5<br>"
                    "<b>Permissions:</b> %6"
                ).arg(info.fileName().toHtmlEscaped())
                 .arg(info.absoluteFilePath().toHtmlEscaped())
                 .arg(sizeStr)
                 .arg(info.isDir() ? "Folder" : mdb.mimeTypeForFile(info).name())
                 .arg(info.lastModified().toString("yyyy-MM-dd HH:mm:ss"))
                 .arg(perms);

                QMessageBox box(window);
                box.setWindowTitle("Properties");
                box.setTextFormat(Qt::RichText);
                box.setText(text);
                box.setIcon(QMessageBox::Information);
                box.exec();
            });
        }
    }

    menu.exec(globalPos);
}
