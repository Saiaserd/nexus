#include "ui/VaultSession.h"

#include <algorithm>

#include <QDir>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include "core/NoteDocument.h"
#include "ui/RecentVaults.h"

namespace nexus::ui {

namespace {

constexpr int kParkPollMs = 100;
constexpr int kShutdownWaitMs = 5000;

} // namespace

QString welcomeNoteText()
{
    return QStringLiteral(
        "# 歡迎使用 Nexus\n"
        "\n"
        "這是你的第一篇筆記。Nexus 把一個資料夾當成 vault，裡面的每個 `.md` 檔就是一篇筆記。\n"
        "\n"
        "## 開始\n"
        "\n"
        "- 在左邊的檔案樹選一篇筆記，或按 Ctrl+N 新增\n"
        "- 用 [[雙括號]] 連結到另一篇筆記\n"
        "- 用 #標籤 為筆記分類\n"
        "- 編輯會在你停止輸入後自動儲存\n"
        "\n"
        "> 用連結的筆記描述意圖，讓 AI 去寫程式。\n");
}

// The watcher of a closed vault (and the NoteFiles whose write listener points at it), kept
// alive until no background task uses the NoteFiles any more.
class VaultSession::Parked : public QObject {
public:
    Parked(std::shared_ptr<core::NoteFiles> files, std::unique_ptr<core::VaultWatcher> watcher,
           QObject* parent)
        : QObject(parent), m_files(std::move(files)), m_watcher(std::move(watcher))
    {
        // A closed vault sends nothing to the UI any more.
        m_watcher->disconnect();
        connect(&m_timer, &QTimer::timeout, this, &Parked::poll);
        m_timer.start(kParkPollMs);
    }

private:
    void poll()
    {
        if (m_files.use_count() > 1) {
            return; // a task still holds the NoteFiles: its write may still call the listener
        }
        m_timer.stop();
        m_files.reset();
        m_watcher.reset();
        deleteLater();
    }

    std::shared_ptr<core::NoteFiles> m_files;
    std::unique_ptr<core::VaultWatcher> m_watcher;
    QTimer m_timer;
};

VaultSession::VaultSession(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<VaultOpenError>("nexus::ui::VaultOpenError");
}

VaultSession::~VaultSession()
{
    // Let running background writes finish before their watcher disappears.
    if (m_futureWatcher) {
        m_futureWatcher->disconnect(this);
    }
    QThreadPool::globalInstance()->waitForDone(kShutdownWaitMs);
    m_files.reset();
    m_watcher.reset();
}

QString VaultSession::name() const
{
    return RecentVaults::displayName(m_root);
}

void VaultSession::open(const QString& path)
{
    start(path, /*create=*/false);
}

void VaultSession::createAndOpen(const QString& path)
{
    start(path, /*create=*/true);
}

void VaultSession::start(const QString& path, bool create)
{
    const QString root = RecentVaults::normalizedPath(path);
    const bool wasOpen = m_state == State::Open;
    release();
    if (wasOpen) {
        m_state = State::Closed;
        m_root.clear();
        m_noteCount = 0;
        m_folderCount = 0;
        emit closed();
    }
    m_state = State::Opening;
    m_pendingPath = root;
    ++m_generation;
    emit opening(root);

    // Everything below touches the disk and therefore runs on a worker thread.
    auto task = [root, create]() -> OpenResult {
        if (create) {
            const QDir dir(root);
            if (!dir.exists()) {
                return unexpected(
                    VaultOpenError{false, {core::FileErrorKind::NotFound, root, std::nullopt}});
            }
            if (!dir.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden |
                             QDir::System)) {
                return unexpected(
                    VaultOpenError{true, {core::FileErrorKind::AlreadyExists, root, std::nullopt}});
            }
            const core::NoteFiles files(root);
            core::NoteDocument doc;
            doc.text = welcomeNoteText();
            if (auto written = files.write(QStringLiteral("歡迎.md"), doc); !written) {
                return unexpected(VaultOpenError{false, written.error()});
            }
        }
        auto vault = core::Vault::open(root);
        if (!vault) {
            return unexpected(VaultOpenError{false, vault.error()});
        }
        return std::move(*vault);
    };

    m_futureWatcher = std::make_unique<QFutureWatcher<OpenResult>>();
    m_futureGeneration = m_generation;
    connect(m_futureWatcher.get(), &QFutureWatcher<OpenResult>::finished, this,
            &VaultSession::onFinished);
    m_futureWatcher->setFuture(QtConcurrent::run(std::move(task)));
}

void VaultSession::onFinished()
{
    if (!m_futureWatcher || m_futureGeneration != m_generation) {
        return;
    }
    OpenResult result = m_futureWatcher->result();
    m_futureWatcher.release()->deleteLater();

    if (!result) {
        m_state = State::Closed;
        const QString path = m_pendingPath;
        m_pendingPath.clear();
        emit openFailed(path, result.error());
        return;
    }

    m_root = m_pendingPath;
    m_noteCount = static_cast<int>(result->notes().size());
    m_folderCount = static_cast<int>(result->folders().size());
    m_snapshot = *result;
    m_files = std::make_shared<core::NoteFiles>(m_root);
    m_watcher = std::make_unique<core::VaultWatcher>(std::move(*result));
    m_files->setWriteListener(m_watcher->writeListener());

    const auto adjust = [this](int delta) {
        m_noteCount = std::max(0, m_noteCount + delta);
        emit noteCountChanged(m_noteCount);
    };
    connect(m_watcher.get(), &core::VaultWatcher::notesAdded, this,
            [adjust](const QStringList& paths) { adjust(static_cast<int>(paths.size())); });
    connect(m_watcher.get(), &core::VaultWatcher::notesRemoved, this,
            [adjust](const QStringList& paths) { adjust(-static_cast<int>(paths.size())); });

    m_state = State::Open;
    emit opened();
}

void VaultSession::release()
{
    // A running open is abandoned: its result is ignored, the worker finishes on its own.
    if (m_futureWatcher) {
        m_futureWatcher->disconnect(this);
        m_futureWatcher.release()->deleteLater();
    }
    if (m_watcher) {
        auto* parked = new Parked(std::move(m_files), std::move(m_watcher), this);
        m_parked.push_back(parked);
        connect(parked, &QObject::destroyed, this, [this, parked] {
            m_parked.erase(std::remove(m_parked.begin(), m_parked.end(), parked), m_parked.end());
        });
    }
    m_files.reset();
    m_watcher.reset();
    m_snapshot = core::Vault();
}

void VaultSession::close()
{
    const bool wasActive = m_state != State::Closed;
    release();
    ++m_generation;
    m_state = State::Closed;
    m_root.clear();
    m_pendingPath.clear();
    m_noteCount = 0;
    m_folderCount = 0;
    if (wasActive) {
        emit closed();
    }
}

} // namespace nexus::ui
