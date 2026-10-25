#pragma once

#include <QDateTime>
#include <QString>

namespace nexus::core {

struct NoteInfo {
    QString relPath; // relative to the vault root, '/' separated
    qint64 size = 0;
    QDateTime modified;

    friend bool operator==(const NoteInfo& a, const NoteInfo& b)
    {
        return a.relPath == b.relPath && a.size == b.size && a.modified == b.modified;
    }
};

} // namespace nexus::core
