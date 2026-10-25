#pragma once

#include <cstdint>
#include <memory>

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include "core/Expected.h"
#include "core/FileError.h"
#include "core/NoteDocument.h"
#include "core/NoteFiles.h"

class QTextDocument;

namespace nexus::ui {

// One open note: its text document, the line ending / BOM it was read with, and autosave
// (ADR 0006). Reading and writing run on QtConcurrent; results come back on the UI thread.
// While a write is in flight further edits only mark the note dirty again; the next write
// starts when the running one has finished.
class NoteSession : public QObject {
    Q_OBJECT

public:
    enum class State : std::uint8_t { Loading, Ready, Failed };
    enum class SaveState : std::uint8_t { Saved, Unsaved, Saving };

    static constexpr int kAutosaveDelayMs = 1000;

    NoteSession(std::shared_ptr<const core::NoteFiles> files, const QString& relPath,
                QObject* parent = nullptr);
    ~NoteSession() override;

    NoteSession(const NoteSession&) = delete;
    NoteSession& operator=(const NoteSession&) = delete;
    NoteSession(NoteSession&&) = delete;
    NoteSession& operator=(NoteSession&&) = delete;

    [[nodiscard]] const QString& relPath() const { return m_relPath; }
    [[nodiscard]] QTextDocument* document() const { return m_document; }
    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] SaveState saveState() const;
    [[nodiscard]] bool isDirty() const { return m_editSerial != m_savedSerial; }
    [[nodiscard]] core::LineEnding lineEnding() const { return m_lineEnding; }
    [[nodiscard]] bool hasBom() const { return m_hasBom; }
    [[nodiscard]] const core::FileError& lastError() const { return m_lastError; }

    // Starts reading the file. Emits loaded() or loadFailed().
    void load();
    // Writes the note now if it has unsaved edits (Ctrl+S, tab switch, focus loss, idle timer).
    void save();
    // Writes any unsaved edits and blocks until done (closing a tab, the vault or the window).
    // Returns false if the write failed.
    bool saveAndWait();
    // The note was renamed or moved on disk (by the file tree).
    void setRelPath(const QString& relPath);

signals:
    void loaded();
    void loadFailed(const nexus::core::FileError& error);
    void saveStateChanged();
    void saveFailed(const nexus::core::FileError& error);
    void relPathChanged(const QString& relPath);

private:
    using ReadResult = Expected<core::NoteDocument, core::FileError>;
    using WriteResult = Expected<void, core::FileError>;

    void onContentsChanged();
    void onReadFinished();
    void onWriteFinished();
    // Takes the result of the write started last; false if it failed. No-op without one.
    bool collectWrite();
    void startWrite();
    [[nodiscard]] core::NoteDocument snapshot() const;

    std::shared_ptr<const core::NoteFiles> m_files;
    QString m_relPath;
    QTextDocument* m_document = nullptr; // child of this
    State m_state = State::Loading;
    core::LineEnding m_lineEnding = core::LineEnding::Lf;
    bool m_hasBom = false;
    core::FileError m_lastError;

    // Every edit bumps m_editSerial; a finished write sets m_savedSerial to the serial it wrote.
    quint64 m_editSerial = 0;
    quint64 m_savedSerial = 0;
    quint64 m_writingSerial = 0;
    bool m_writePending = false;
    bool m_settingText = false;

    QTimer m_autosave;
    QFutureWatcher<ReadResult> m_read;
    QFutureWatcher<WriteResult> m_write;
};

} // namespace nexus::ui
