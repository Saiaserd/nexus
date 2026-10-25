#include <QApplication>
#include <QStringList>
#include <QTimer>

#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    const QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Nexus"));

    // Smoke-test hook (used by ctest): show the main window, run the event loop
    // once, then exit. Lets sanitizers cover the window's whole lifetime.
    const bool quitAfterShow =
        QCoreApplication::arguments().contains(QStringLiteral("--quit-after-show"));

    nexus::ui::MainWindow window;
    window.show();

    if (quitAfterShow) {
        QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
    }

    return QApplication::exec();
}
