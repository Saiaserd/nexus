#include <QCoreApplication>
#include <QTimer>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Nexus"));

    // No window yet (T-006). Enter the event loop once and leave immediately.
    QTimer::singleShot(0, &app, &QCoreApplication::quit);
    return QCoreApplication::exec();
}
