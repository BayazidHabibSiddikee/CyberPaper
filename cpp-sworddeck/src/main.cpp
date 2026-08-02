#include "cyberdeck.h"
#include <QApplication>
#include <QScreen>
#include <csignal>

static void sigtermHandler(int) { QApplication::quit(); }

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("sworddeck");

    std::signal(SIGTERM, sigtermHandler);

    int sw = 1920, sh = 1080;
    for (int i = 1; i < argc - 1; i++) {
        if (QString(argv[i]) == "--screen") {
            QStringList dim = QString(argv[i + 1]).split('x');
            if (dim.size() == 2) { sw = dim[0].toInt(); sh = dim[1].toInt(); }
        }
    }
    if (sw == 1920 && sh == 1080) {
        if (auto *screen = app.primaryScreen()) {
            sw = screen->size().width();
            sh = screen->size().height();
        }
    }

    CyberDeck deck(sw, sh);
    deck.show();
    deck.showBottomBar();

    return app.exec();
}
