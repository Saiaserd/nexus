#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <QFutureWatcher>
#include <QObject>
#include <QString>

#include "core/Expected.h"
#include "core/FileError.h"
#include "core/NoteFiles.h"
#include "core/Vault.h"
#include "core/VaultWatcher.h"

namespace nexus::ui {

// Why a vault could not be opened or created.
struct VaultOpenError {
    bool folderNotEmpty = false; // create only: the chosen folder already has content
    core::FileError file;
};

// Holds the vault that is currently open: its snapshot, the watcher and the NoteFiles used by
// background tasks. Opening and creating run on QtConcurrent; results come back as signals.
//
// Lifetime rule (core/VaultWatcher.h): NoteFiles::setWriteListener captured the watcher's
// `this`, so the watcher must outlive every background write. files() hands out a shared_ptr;
// when the vault is closed the watcher is parked until every holder has released it, and only
// then destroyed.
class VaultSession : public QObject {
    Q_OBJECT

public:
    enum class State : std::uint8_t { Closed, Opening, Open };

    explicit VaultSession(QObject* parent = nullptr);
    ~VaultSession() override;

    VaultSession(const VaultSession&) = delete;
    VaultSession& operator=(const VaultSession&) = delete;
    VaultSession(VaultSession&&) = delete;
    VaultSession& operator=(VaultSession&&) = delete;

    [[nodiscard]] State state() const { return m_state; }
    [[nodiscard]] bool isOpen() const { return m_state == State::Open; }
    [[nodiscard]] const QString& root() const { return m_root; }
    [[nodiscard]] QString name() const;
    [[nodiscard]] int noteCount() const { return m_noteCount; }
    [[nodiscard]] int folderCount() const { return m_folderCount; }
    // Valid while open; null otherwise. Background tasks keep the shared_ptr while they use it.
    [[nodiscard]] std::shared_ptr<const core::NoteFiles> files() const { return m_files; }
    [[nodiscard]] core::VaultWatcher* watcher() const { return m_watcher.get(); }
    // Closed vaults whose watcher still waits for background tasks (for tests).
    [[nodiscard]] int parkedCount() const { return static_cast<int>(m_parked.size()); }

public slots:
    // Opens an existing folder. Any vault that is open (or still opening) is replaced.
    void open(const QString& path);
    // Writes a welcome note into an empty folder and opens it as a vault.
    void createAndOpen(const QString& path);
    void close();

signals:
    void opening(const QString& path);
    void opened();
    void openFailed(const QString& path, const nexus::ui::VaultOpenError& error);
    void closed();
    void noteCountChanged(int count);

private:
    using OpenResult = Expected<core::Vault, VaultOpenError>;
    class Parked;

    void start(const QString& path, bool create);
    void onFinished();
    void release();

    State m_state = State::Closed;
    QString m_root;
    QString m_pendingPath;
    quint64 m_generation = 0;
    int m_noteCount = 0;
    int m_folderCount = 0;

    std::shared_ptr<core::NoteFiles> m_files;
    std::unique_ptr<core::VaultWatcher> m_watcher;
    std::vector<Parked*> m_parked;
    // Replaced on every start(); a superseded result is ignored through its generation number.
    std::unique_ptr<QFutureWatcher<OpenResult>> m_futureWatcher;
    quint64 m_futureGeneration = 0;
};

// The text of the note created in a new vault.
QString welcomeNoteText();

} // namespace nexus::ui

Q_DECLARE_METATYPE(nexus::ui::VaultOpenError)
