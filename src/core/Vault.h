#pragma once

#include <vector>

#include <QSet>
#include <QString>

#include "core/Expected.h"
#include "core/FileError.h"
#include "core/NoteInfo.h"

namespace nexus::core {

// A snapshot of a folder opened as a vault: every .md note and every folder (including empty
// ones). Hidden entries (names starting with '.') are skipped and directory symlinks are not
// followed. Paths are vault-relative and '/' separated (ADR 0004). A Vault is a plain value;
// re-open to refresh.
class Vault {
public:
    Vault() = default;

    static Expected<Vault, FileError> open(const QString& root);

    [[nodiscard]] const QString& root() const { return m_root; }
    [[nodiscard]] const std::vector<NoteInfo>& notes() const { return m_notes; }
    [[nodiscard]] const std::vector<QString>& folders() const { return m_folders; }

    // Absolute path for a vault-relative path; empty if rel is not a safe relative path.
    [[nodiscard]] QString absolutePath(const QString& rel) const;
    // True if rel is a known note or folder of this snapshot.
    [[nodiscard]] bool contains(const QString& rel) const;

private:
    QString m_root;
    std::vector<NoteInfo> m_notes;
    std::vector<QString> m_folders;
    QSet<QString> m_known;
};

} // namespace nexus::core
