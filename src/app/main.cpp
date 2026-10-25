#include <QApplication>
#include <QSettings>
#include <QStringList>
#include <QTimer>

#include "ui/CrashDialogs.h"
#include "ui/DevOptions.h"
#include "ui/MainWindow.h"
#include "ui/ThemeManager.h"

int main(int argc, char* argv[])
{
    nexus::ui::suppressCrashDialogsIfAutomated(argc, argv);
    const QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Nexus"));
    QApplication::setOrganizationName(QStringLiteral("Nexus"));

    // Smoke-test hook (used by ctest): show the main window, run the event loop
    // once, then exit. Lets sanitizers cover the window's whole lifetime.
    const bool quitAfterShow =
        QCoreApplication::arguments().contains(QStringLiteral("--quit-after-show"));

    QSettings settings;
    nexus::ui::ThemeManager theme(settings);
    nexus::ui::MainWindow window(theme);
    window.show();
    nexus::ui::applyDeveloperOptions(QApplication::arguments(), theme, window);

    if (quitAfterShow) {
        QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
    }

    return QApplication::exec();
}
