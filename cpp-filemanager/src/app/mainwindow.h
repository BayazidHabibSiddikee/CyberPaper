#pragma once
#include <QMainWindow>
#include <QDate>
#include <QSplitter>
#include <QStack>
#include <QKeyEvent>

class ToolBar;
class SideBar;
class FileView;
class StatusBar;
class PreviewPanel;
class QFileSystemModel;
class QModelIndex;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &startPath = QString(), QWidget *parent = nullptr);

    int markCount() const;
    bool allMarked(const QStringList &paths) const;

public slots:
    void navigateTo(const QString &path);
    void navigateUp();
    void navigateBack();
    void navigateForward();
    void navigateHome();
    void refresh();
    void openFile(const QString &path);
    void previewSelected();
    void graphSelected();
    void graphCurrent();
    void selectAll();
    void copySelection();
    void cutSelection();
    void pasteClipboard();
    void deleteSelection();
    void renameSelected();
    void createNewFolder();
    void createNewFile();
    void toggleHidden();
    void toggleViewMode();
    void search(const QString &query);
    void bookmarkCurrent();
    void bookmarkSelection();
    void openTerminalHere();
    void compressSelection(const QString &formatId);
    void extractSelectionHere();
    void extractSelectionToFolder();
    void toggleMarkSelection();
    void clearMarks();

private slots:
    void onFileActivated(const QModelIndex &index);
    void onSelectionChanged();

private:
    void setupUi();
    void setupMenus();
    void applyTypeDateFilter();
    void updateStatusBar();
    void updateNavButtons();
    void applyDirectory(const QString &path, bool pushHistory);
    void setClipboard(const QStringList &paths, bool cut);
    void showPreview(const QString &path);
    void widenPreviewPane();
    void graphFolder(const QString &path);
    QStringList actionPaths() const;
    int currentItemCount() const;

    ToolBar *m_toolbar = nullptr;
    SideBar *m_sidebar = nullptr;
    FileView *m_fileView = nullptr;
    PreviewPanel *m_preview = nullptr;
    StatusBar *m_statusbar = nullptr;
    QSplitter *m_splitter = nullptr;

    QString m_currentPath;
    QString m_pendingRoot;
    int m_typeFilter = 0;
    QDate m_dateFrom;
    QDate m_dateTo;
    QStack<QString> m_backStack;
    QStack<QString> m_forwardStack;
    QFileSystemModel *m_fsModel = nullptr;

    QStringList m_clipboard;
    bool m_clipboardIsCut = false;
    bool m_showHidden = false;
};
