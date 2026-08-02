#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QToolButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QStyle>
#include <QDate>

class ToolBar : public QWidget {
    Q_OBJECT
public:
    explicit ToolBar(QWidget *parent = nullptr);

    void setPath(const QString &path);
    void focusPath();
    void setCanGoBack(bool on);
    void setCanGoForward(bool on);
    void setDetailsMode(bool details);

signals:
    void pathEntered(const QString &path);
    void goBack();
    void goForward();
    void goUp();
    void goHome();
    void refreshRequested();
    void searchQuery(const QString &query);
    void viewModeToggled();
    void typeFilterChanged(int type);
    void dateRangeChanged(const QDate &from, const QDate &to);

private:
    QToolButton *makeNavButton(const QString &themeIcon, QStyle::StandardPixmap fallback,
                               const QString &tip);
    void pickDateRange();

    QLineEdit *m_pathEdit;
    QLineEdit *m_searchEdit;
    QComboBox *m_typeBox;
    QToolButton *m_dateBtn;
    QToolButton *m_backBtn;
    QToolButton *m_forwardBtn;
    QToolButton *m_upBtn;
    QToolButton *m_homeBtn;
    QToolButton *m_refreshBtn;
    QToolButton *m_viewBtn;

    QDate m_from;
    QDate m_to;
};
