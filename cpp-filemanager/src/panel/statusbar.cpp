#include "panel/statusbar.h"
#include "app/theme.h"

#include <QHBoxLayout>

static QString formatSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 1);
}

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(26);
    setStyleSheet(QString(
        "StatusBar { background: %1; border-top: 1px solid %2; }"
        "QLabel { color: %3; font-size: 12px; padding: 0 10px; }"
    ).arg(Theme::BG2, Theme::BORDER, Theme::CYAN));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_infoLabel = new QLabel(this);
    m_searchLabel = new QLabel(this);
    m_markLabel = new QLabel(this);
    m_selectionLabel = new QLabel(this);
    m_clipboardLabel = new QLabel(this);
    m_searchLabel->setStyleSheet(QString("color: %1;").arg(Theme::PURPLE));
    m_markLabel->setStyleSheet(QString("color: %1; font-weight: 600;").arg(Theme::AMBER));
    m_selectionLabel->setStyleSheet(QString("color: %1;").arg(Theme::FG));
    m_clipboardLabel->setStyleSheet(QString("color: %1;").arg(Theme::GREEN));

    layout->addWidget(m_infoLabel, 1);
    layout->addWidget(m_searchLabel);
    layout->addWidget(m_markLabel);
    layout->addWidget(m_selectionLabel);
    layout->addWidget(m_clipboardLabel);
}

void StatusBar::setSearchInfo(const QString &text) {
    m_searchLabel->setText(text);
}

void StatusBar::updateInfo(int itemCount, int selectedCount, qint64 selectedSize,
                           int clipboardCount, bool isCut, int markCount) {
    m_infoLabel->setText(QString("%1 item%2")
                             .arg(itemCount)
                             .arg(itemCount == 1 ? "" : "s"));

    m_markLabel->setText(markCount > 0
                             ? QString("✓ %1 marked").arg(markCount)
                             : QString());

    if (selectedCount > 0) {
        QString info = QString("%1 selected").arg(selectedCount);
        if (selectedSize > 0)
            info += QString("  (%1)").arg(formatSize(selectedSize));
        m_selectionLabel->setText(info);
    } else {
        m_selectionLabel->setText(QString());
    }

    if (clipboardCount > 0) {
        m_clipboardLabel->setText(QString("%1 %2")
                                      .arg(isCut ? "Cut:" : "Copied:")
                                      .arg(clipboardCount));
    } else {
        m_clipboardLabel->setText(QString());
    }
}
