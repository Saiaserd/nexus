#pragma once

#include <functional>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>

namespace nexus::test {

// Runs the event loop until `done()` is true or the timeout passes; returns the final value.
inline bool waitFor(const std::function<bool()>& done, int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return true;
}

// Writes a small file (creating folders); returns false on failure.
inline bool writeFile(const QString& path, const QByteArray& content = "# note\n")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

} // namespace nexus::test
