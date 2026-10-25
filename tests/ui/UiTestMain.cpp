#include <QApplication>
#include <QtGlobal>
#include <gtest/gtest.h>

#include "ui/CrashDialogs.h"

// Widgets tests need a QApplication; the offscreen platform works without a display (CI, ctest).
int main(int argc, char* argv[])
{
    nexus::ui::suppressCrashDialogs();
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    const QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
