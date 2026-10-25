#pragma once

#include <functional>
#include <optional>
#include <utility>

#include <QString>

#include "core/Expected.h"
#include "core/FileError.h"
#include "core/NoteDocument.h"
#include "core/NoteInfo.h"

namespace nexus::core {

// Checks one file or folder name (no path separators). Used for live validation while the
// user types, so it is cheap and has no side effects. Length is limited to 250 UTF-8 bytes
// so that a ".md" suffix still fits the common 255-byte limit.
[[nodiscard]] std::optional<NameError> validateName(const QString& name);

// File operations of one vault. All paths are vault-relative and '/' separated; "" means the
// vault root where a folder is expected. Operations are synchronous (call from a worker
// thread, never from the UI thread).
class NoteFiles {
public:
    // Called after a successful write() with the file's new size and modification time, so a
    // VaultWatcher can ignore the change Nexus made itself.
    using WriteListener = std::function<void(const NoteInfo&)>;

    explicit NoteFiles(const QString& vaultRoot);

    void setWriteListener(WriteListener listener) { m_listener = std::move(listener); }

    [[nodiscard]] Expected<NoteDocument, FileError> read(const QString& rel) const;
    [[nodiscard]] Expected<void, FileError> write(const QString& rel,
                                                  const NoteDocument& doc) const;

    // Creates an empty "<baseName>.md"; if the name is taken, "<baseName> 1.md", "... 2.md".
    [[nodiscard]] Expected<QString, FileError>
    createNote(const QString& folderRel, const QString& baseName = QStringLiteral("未命名")) const;
    [[nodiscard]] Expected<QString, FileError> createFolder(const QString& parentRel,
                                                            const QString& name) const;
    // Renames a note (the original extension is kept) or a folder; returns the new path.
    // Changing only the letter case works on every platform.
    [[nodiscard]] Expected<QString, FileError> rename(const QString& rel,
                                                      const QString& newBaseName) const;
    // Moves to the recycle bin; fails with TrashFailed where unsupported (never deletes).
    [[nodiscard]] Expected<void, FileError> moveToTrash(const QString& rel) const;

private:
    QString m_root;
    WriteListener m_listener;
};

} // namespace nexus::core
