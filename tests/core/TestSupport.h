#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace nexus::test {

inline bool makeDir(const QString& root, const QString& rel)
{
    return QDir(root).mkpath(rel);
}

inline bool writeFile(const QString& root, const QString& rel, const QByteArray& content = {})
{
    const QString abs = root + u'/' + rel;
    if (!QDir().mkpath(QFileInfo(abs).absolutePath())) {
        return false;
    }
    QFile file(abs);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    return file.write(content) == content.size();
}

inline QByteArray readFile(const QString& root, const QString& rel)
{
    QFile file(root + u'/' + rel);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

inline bool exists(const QString& root, const QString& rel)
{
    return QFileInfo::exists(root + u'/' + rel);
}

} // namespace nexus::test
