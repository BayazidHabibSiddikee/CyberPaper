#include "app/mainwindow.h"
#include "app/theme.h"
#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QPalette>
#include <QColor>
#include <QSettings>

// Qt only inherits the desktop icon theme when a platform theme plugin is
// active; under Fusion on a bare i3 session QIcon::themeName() comes back
// empty or "hicolor", so every fromTheme() lookup yields a null icon and the
// file list renders bare. Resolve the GTK theme setting ourselves instead.
static void applyIconTheme() {
    const QString current = QIcon::themeName();
    if (!current.isEmpty() && current != QLatin1String("hicolor"))
        return;

    QStringList candidates;
    const QString gtkIni = QDir::homePath() + "/.config/gtk-3.0/settings.ini";
    if (QFile::exists(gtkIni)) {
        QSettings s(gtkIni, QSettings::IniFormat);
        candidates << s.value("Settings/gtk-icon-theme-name").toString().trimmed();
    }
    candidates << "Papirus-Dark" << "Papirus" << "breeze-dark" << "Adwaita";

    for (const QString &name : candidates) {
        if (name.isEmpty())
            continue;
        for (const QString &dir : QIcon::themeSearchPaths()) {
            if (QFile::exists(dir + "/" + name + "/index.theme")) {
                QIcon::setThemeName(name);
                return;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("swordfm");
    app.setApplicationDisplayName("SwordFM");
    app.setOrganizationName("sword");
    app.setDesktopFileName("swordfm");

    if (QStyleFactory::keys().contains("Fusion", Qt::CaseInsensitive))
        app.setStyle(QStyleFactory::create("Fusion"));

    applyIconTheme();

    // One Dark palette (matches sworddeck)
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(Theme::BG));
    pal.setColor(QPalette::WindowText, QColor(Theme::FG));
    pal.setColor(QPalette::Base, QColor(Theme::BG));
    pal.setColor(QPalette::AlternateBase, QColor(Theme::BG2));
    pal.setColor(QPalette::Text, QColor(Theme::FG));
    pal.setColor(QPalette::Button, QColor(Theme::DIM));
    pal.setColor(QPalette::ButtonText, QColor(Theme::FG));
    pal.setColor(QPalette::Highlight, QColor(Theme::DIM));
    pal.setColor(QPalette::HighlightedText, QColor(Theme::CYAN));
    pal.setColor(QPalette::ToolTipBase, QColor(Theme::BG2));
    pal.setColor(QPalette::ToolTipText, QColor(Theme::FG));
    pal.setColor(QPalette::PlaceholderText, QColor(Theme::FG_DIM));
    pal.setColor(QPalette::Link, QColor(Theme::CYAN));
    pal.setColor(QPalette::BrightText, QColor(Theme::RED));
    app.setPalette(pal);
    app.setStyleSheet(Theme::appStylesheet());

    QString startPath;
    if (argc > 1)
        startPath = QDir::fromNativeSeparators(QString::fromLocal8Bit(argv[1]));

    MainWindow w(startPath);
    w.show();
    return app.exec();
}
