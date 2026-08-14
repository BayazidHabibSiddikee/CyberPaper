#include "panel/toolbar.h"
#include "app/theme.h"

#include <QIcon>
#include <QApplication>
#include <QStyle>
#include <QDialog>
#include <QDateEdit>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>

ToolBar::ToolBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(42);
    setStyleSheet(QString(
        "ToolBar { background: %1; border-bottom: 1px solid %2; }"
        "QToolButton {"
        "  background: transparent; border: none; border-radius: 4px;"
        "  padding: 4px; margin: 1px; color: %3;"
        "}"
        "QToolButton:hover { background: %2; }"
        "QToolButton:pressed { background: %4; }"
        "QToolButton:disabled { color: %5; }"
        "QLineEdit {"
        "  background: %4; color: %3; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 10px; font-size: 13px;"
        "  selection-background-color: %2; selection-color: %6;"
        "}"
        "QLineEdit:focus { border-color: %6; }"
        "QComboBox {"
        "  background: %4; color: %3; border: 1px solid %2;"
        "  border-radius: 4px; padding: 4px 8px; font-size: 12px;"
        "}"
        "QComboBox:hover { border-color: %6; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView {"
        "  background: %1; color: %3; border: 1px solid %2;"
        "  selection-background-color: %2; selection-color: %6;"
        "}"
    ).arg(Theme::BG2, Theme::DIM, Theme::FG, Theme::BG, Theme::FG_DIM, Theme::CYAN));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(2);

    m_backBtn = makeNavButton("go-previous", QStyle::SP_ArrowBack, "Back (Alt+Left)");
    m_forwardBtn = makeNavButton("go-next", QStyle::SP_ArrowForward, "Forward (Alt+Right)");
    m_upBtn = makeNavButton("go-up", QStyle::SP_ArrowUp, "Up (Alt+Up)");
    m_homeBtn = makeNavButton("go-home", QStyle::SP_DirHomeIcon, "Home");
    m_refreshBtn = makeNavButton("view-refresh", QStyle::SP_BrowserReload, "Refresh (F5)");

    connect(m_backBtn, &QToolButton::clicked, this, &ToolBar::goBack);
    connect(m_forwardBtn, &QToolButton::clicked, this, &ToolBar::goForward);
    connect(m_upBtn, &QToolButton::clicked, this, &ToolBar::goUp);
    connect(m_homeBtn, &QToolButton::clicked, this, &ToolBar::goHome);
    connect(m_refreshBtn, &QToolButton::clicked, this, &ToolBar::refreshRequested);

    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText("Location… (Ctrl+L)");
    m_pathEdit->setClearButtonEnabled(true);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, [this]() {
        emit pathEntered(m_pathEdit->text().trimmed());
    });

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search…");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMaximumWidth(180);
    m_searchEdit->setMinimumWidth(120);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ToolBar::searchQuery);

    m_typeBox = new QComboBox(this);
    m_typeBox->addItem("All types");
    m_typeBox->addItem("Images");
    m_typeBox->addItem("Videos");
    m_typeBox->addItem("Audio");
    m_typeBox->addItem("Documents");
    m_typeBox->addItem("Archives");
    m_typeBox->addItem("Discs / ISO");
    m_typeBox->setToolTip("Filter by file type");
    m_typeBox->setFixedWidth(120);
    connect(m_typeBox, &QComboBox::currentIndexChanged, this, &ToolBar::typeFilterChanged);

    m_dateBtn = makeNavButton("x-office-calendar", QStyle::SP_FileDialogDetailedView,
                              "Filter by modified date range");
    connect(m_dateBtn, &QToolButton::clicked, this, &ToolBar::pickDateRange);

    m_viewBtn = makeNavButton("view-list-details", QStyle::SP_FileDialogDetailedView,
                              "Toggle Icon / Details view");
    connect(m_viewBtn, &QToolButton::clicked, this, &ToolBar::viewModeToggled);

    layout->addWidget(m_backBtn);
    layout->addWidget(m_forwardBtn);
    layout->addWidget(m_upBtn);
    layout->addWidget(m_homeBtn);
    layout->addWidget(m_refreshBtn);
    layout->addSpacing(6);
    layout->addWidget(m_pathEdit, 1);
    layout->addSpacing(4);
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_typeBox);
    layout->addWidget(m_dateBtn);
    layout->addWidget(m_viewBtn);

    setCanGoBack(false);
    setCanGoForward(false);
}

void ToolBar::pickDateRange() {
    QDialog dlg(this);
    dlg.setWindowTitle("Filter by modified date");

    auto *fromEdit = new QDateEdit(m_from.isValid() ? m_from
                                                    : QDate::currentDate().addMonths(-1), &dlg);
    auto *toEdit = new QDateEdit(m_to.isValid() ? m_to : QDate::currentDate(), &dlg);
    for (auto *e : {fromEdit, toEdit}) {
        e->setCalendarPopup(true);
        e->setDisplayFormat("yyyy-MM-dd");
    }

    auto *enabled = new QCheckBox("Limit to this date range", &dlg);
    enabled->setChecked(m_from.isValid() || m_to.isValid());

    auto *form = new QFormLayout;
    form->addRow("From:", fromEdit);
    form->addRow("To:", toEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *lay = new QVBoxLayout(&dlg);
    lay->addWidget(enabled);
    lay->addLayout(form);
    lay->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    if (enabled->isChecked()) {
        m_from = fromEdit->date();
        m_to = toEdit->date();
        if (m_from > m_to)
            std::swap(m_from, m_to);
    } else {
        m_from = QDate();
        m_to = QDate();
    }

    m_dateBtn->setToolTip(m_from.isValid()
        ? QString("Modified %1 → %2").arg(m_from.toString("yyyy-MM-dd"), m_to.toString("yyyy-MM-dd"))
        : QString("Filter by modified date range"));
    emit dateRangeChanged(m_from, m_to);
}

QToolButton *ToolBar::makeNavButton(const QString &themeIcon, QStyle::StandardPixmap fallback,
                                    const QString &tip) {
    auto *btn = new QToolButton(this);
    QIcon icon = QIcon::fromTheme(themeIcon);
    if (icon.isNull())
        icon = QApplication::style()->standardIcon(fallback);
    btn->setIcon(icon);
    btn->setIconSize(QSize(20, 20));
    btn->setToolTip(tip);
    btn->setAutoRaise(true);
    btn->setFixedSize(32, 32);
    return btn;
}

void ToolBar::setPath(const QString &path) {
    if (m_pathEdit->text() != path)
        m_pathEdit->setText(path);
}

void ToolBar::focusPath() {
    m_pathEdit->setFocus();
    m_pathEdit->selectAll();
}

void ToolBar::setCanGoBack(bool on) {
    m_backBtn->setEnabled(on);
}

void ToolBar::setCanGoForward(bool on) {
    m_forwardBtn->setEnabled(on);
}

void ToolBar::setDetailsMode(bool details) {
    QIcon icon = QIcon::fromTheme(details ? "view-grid-symbolic" : "view-list-details");
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(
            details ? QStyle::SP_FileDialogListView : QStyle::SP_FileDialogDetailedView);
    }
    m_viewBtn->setIcon(icon);
}
