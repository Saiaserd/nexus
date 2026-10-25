// Custom gtest entry point: tests that use QObject / QFileSystemWatcher / signals need a
// QCoreApplication (and its event loop). Every test in nexus_core_tests runs under this one.
#include <gtest/gtest.h>

#include <QCoreApplication>

int main(int argc, char** argv)
{
    const QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
