#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    const QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Nexus"));

    nexus::ui::MainWindow window;
    window.show();

    return QApplication::exec();
}
