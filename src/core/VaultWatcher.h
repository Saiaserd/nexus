#pragma once

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include "core/Expected.h"
#include "core/FileError.h"
#include "core/NoteFiles.h"
#include "core/NoteInfo.h"
#include "core/Vault.h"

namespace nexus::core {

// Watches a vault for changes made outside Nexus and reports them as signals.
//
// File-system events (QFileSystemWatcher on every folder and note) only start a debounce
// timer; the actual change set is computed by re-scanning the vault on a worker thread
// (QtConcurrent) and diffing against the previous snapshot, so the UI thread never walks the
// disk. A rename shows up as one removed plus one added path. Paths are vault-relative.
//
// Changes Nexus makes itself are filtered out: NoteFiles::write reports the new size and
// mtime through writeListener(), and a note whose scanned size and mtime equal the
// registered ones does not produce notesModified. Narrow race: a scan that completes between
// the write's commit and its registration reports a spurious notesModified.
//
// Must live in a thread with an event loop. expectWrite() may be called from any thread.
class VaultWatcher : public QObject {
    Q_OBJECT

public:
    static constexpr int kDefaultDebounceMs = 150;

    explicit VaultWatcher(Vault vault, QObject* parent = nullptr);
    ~VaultWatcher() override;

    VaultWatcher(const VaultWatcher&) = delete;
    VaultWatcher& operator=(const VaultWatcher&) = delete;
    VaultWatcher(VaultWatcher&&) = delete;
    VaultWatcher& operator=(VaultWatcher&&) = delete;

    void setDebounceMs(int ms);
    [[nodiscard]] int debounceMs() const { return m_debounce.interval(); }

    // Registers the size / mtime of a file Nexus just wrote so the change is ignored.
    void expectWrite(const NoteInfo& info);
    // A listener to hand to NoteFiles::setWriteListener. The watcher must outlive the NoteFiles.
    [[nodiscard]] NoteFiles::WriteListener writeListener();

public slots:
    // Re-scans now (skips the debounce); useful after bulk operations and in tests.
    void rescanNow();

signals:
    void notesAdded(const QStringList& relPaths);
    void notesRemoved(const QStringList& relPaths);
    void notesModified(const QStringList& relPaths);
    void foldersChanged();
    // Emitted after every finished re-scan, whether or not something changed.
    void rescanned();

private slots:
    void onFileSystemEvent();
    void onScanFinished();
    void pumpWatchQueue();

private:
    using ScanResult = Expected<Vault, FileError>;

    void startScan();
    void applySnapshot(const Vault& next);
    void syncWatches();
    void scheduleScan();

    Vault m_vault;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QFutureWatcher<ScanResult> m_scanWatcher;
    bool m_scanning = false;
    bool m_scanAgain = false;

    QStringList m_watchQueue;
    bool m_watchAdded = false;
    bool m_pumpQueued = false;

    QMutex m_expectedMutex;
    QHash<QString, NoteInfo> m_expected;
};

} // namespace nexus::core
