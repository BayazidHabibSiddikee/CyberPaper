#pragma once
#include <QWidget>
#include <QLabel>

class StatusBar : public QWidget {
    Q_OBJECT
public:
    explicit StatusBar(QWidget *parent = nullptr);

    void updateInfo(int itemCount, int selectedCount, qint64 selectedSize,
                    int clipboardCount, bool isCut, int markCount = 0);
    void setSearchInfo(const QString &text);

private:
    QLabel *m_infoLabel;
    QLabel *m_searchLabel;
    QLabel *m_markLabel;
    QLabel *m_selectionLabel;
    QLabel *m_clipboardLabel;
};
