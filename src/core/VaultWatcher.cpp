#include "core/VaultWatcher.h"

#include <QMutexLocker>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

namespace nexus::core {

namespace {

constexpr int kWatchBatchSize = 256;

} // namespace

VaultWatcher::VaultWatcher(Vault vault, QObject* parent)
    : QObject(parent), m_vault(std::move(vault)), m_watcher(this)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDefaultDebounceMs);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &VaultWatcher::onFileSystemEvent);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &VaultWatcher::onFileSystemEvent);
    connect(&m_debounce, &QTimer::timeout, this, &VaultWatcher::startScan);
    connect(&m_scanWatcher, &QFutureWatcher<ScanResult>::finished, this,
            &VaultWatcher::onScanFinished);

    syncWatches();
}

VaultWatcher::~VaultWatcher()
{
    m_scanWatcher.disconnect(this);
    m_scanWatcher.waitForFinished();
}

void VaultWatcher::setDebounceMs(int ms)
{
    m_debounce.setInterval(ms);
}

void VaultWatcher::expectWrite(const NoteInfo& info)
{
    const QMutexLocker lock(&m_expectedMutex);
    m_expected.insert(info.relPath, info);
}

NoteFiles::WriteListener VaultWatcher::writeListener()
{
    return [this](const NoteInfo& info) { expectWrite(info); };
}

void VaultWatcher::rescanNow()
{
    m_debounce.stop();
    startScan();
}

void VaultWatcher::onFileSystemEvent()
{
    // Start the window on the first event only, so a steady stream cannot postpone it forever.
    if (!m_debounce.isActive()) {
        m_debounce.start();
    }
}

void VaultWatcher::scheduleScan()
{
    if (!m_debounce.isActive()) {
        m_debounce.start();
    }
}

void VaultWatcher::startScan()
{
    if (m_scanning) {
        m_scanAgain = true;
        return;
    }
    m_scanning = true;
    const QString root = m_vault.root();
    m_scanWatcher.setFuture(QtConcurrent::run([root] { return Vault::open(root); }));
}

void VaultWatcher::onScanFinished()
{
    m_scanning = false;
    const ScanResult result = m_scanWatcher.result();
    // If the root disappeared or became unreadable there is nothing sensible to diff; keep the
    // old snapshot and let a later event retry.
    if (result.has_value()) {
        applySnapshot(*result);
        syncWatches();
    }
    emit rescanned();

    if (m_scanAgain) {
        m_scanAgain = false;
        startScan();
    }
}

void VaultWatcher::applySnapshot(const Vault& next)
{
    QHash<QString, const NoteInfo*> before;
    before.reserve(static_cast<qsizetype>(m_vault.notes().size()));
    for (const NoteInfo& note : m_vault.notes()) {
        before.insert(note.relPath, &note);
    }

    QStringList added;
    QStringList modified;
    QSet<QString> still;
    {
        const QMutexLocker lock(&m_expectedMutex);
        for (const NoteInfo& note : next.notes()) {
            const auto it = before.constFind(note.relPath);
            if (it == before.constEnd()) {
                added << note.relPath;
                m_expected.remove(note.relPath);
                continue;
            }
            still.insert(note.relPath);
            const NoteInfo& old = **it;
            if (old.size == note.size && old.modified == note.modified) {
                continue;
            }
            const auto expected = m_expected.find(note.relPath);
            const bool ours = expected != m_expected.end() && expected->size == note.size &&
                              expected->modified == note.modified;
            if (expected != m_expected.end()) {
                m_expected.erase(expected);
            }
            if (!ours) {
                modified << note.relPath;
            }
        }
    }
    QStringList removed;
    for (const NoteInfo& note : m_vault.notes()) {
        if (!still.contains(note.relPath)) {
            removed << note.relPath;
        }
    }

    const bool foldersDiffer = next.folders() != m_vault.folders();
    m_vault = next;

    if (!removed.isEmpty()) {
        emit notesRemoved(removed);
    }
    if (!added.isEmpty()) {
        emit notesAdded(added);
    }
    if (!modified.isEmpty()) {
        emit notesModified(modified);
    }
    if (foldersDiffer) {
        emit foldersChanged();
    }
}

void VaultWatcher::syncWatches()
{
    QSet<QString> desired;
    desired.insert(m_vault.root());
    for (const QString& folder : m_vault.folders()) {
        desired.insert(m_vault.absolutePath(folder));
    }
    for (const NoteInfo& note : m_vault.notes()) {
        desired.insert(m_vault.absolutePath(note.relPath));
    }

    QStringList stale;
    QSet<QString> current;
    for (const QString& path : m_watcher.directories() + m_watcher.files()) {
        current.insert(path);
        if (!desired.contains(path)) {
            stale << path;
        }
    }
    if (!stale.isEmpty()) {
        m_watcher.removePaths(stale);
    }

    // Registering thousands of paths is I/O: do it in small batches between event-loop turns.
    m_watchQueue.clear();
    for (const QString& path : desired) {
        if (!current.contains(path)) {
            m_watchQueue << path;
        }
    }
    if (!m_watchQueue.isEmpty() && !m_pumpQueued) {
        m_pumpQueued = true;
        QMetaObject::invokeMethod(this, &VaultWatcher::pumpWatchQueue, Qt::QueuedConnection);
    }
}

void VaultWatcher::pumpWatchQueue()
{
    m_pumpQueued = false;
    if (!m_watchQueue.isEmpty()) {
        const qsizetype count = std::min<qsizetype>(kWatchBatchSize, m_watchQueue.size());
        const QStringList batch = m_watchQueue.mid(m_watchQueue.size() - count);
        m_watchQueue.erase(m_watchQueue.end() - count, m_watchQueue.end());
        const QStringList failed = m_watcher.addPaths(batch);
        if (failed.size() < batch.size()) {
            m_watchAdded = true;
        }
    }
    if (!m_watchQueue.isEmpty()) {
        m_pumpQueued = true;
        QMetaObject::invokeMethod(this, &VaultWatcher::pumpWatchQueue, Qt::QueuedConnection);
        return;
    }
    if (m_watchAdded) {
        // Anything that changed between the snapshot and the moment its watch was registered
        // would be missed, so verify once more with a scan.
        m_watchAdded = false;
        scheduleScan();
    }
}

} // namespace nexus::core
