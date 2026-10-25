#include <gtest/gtest.h>

#include <memory>

#include <QDir>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>

#include "TestSupport.h"
#include "core/NoteFiles.h"
#include "core/Vault.h"
#include "core/VaultWatcher.h"

using nexus::core::LineEnding;
using nexus::core::NoteDocument;
using nexus::core::NoteFiles;
using nexus::core::Vault;
using nexus::core::VaultWatcher;
using nexus::test::makeDir;
using nexus::test::writeFile;

namespace {

// Upper bound only: every wait returns as soon as the signal arrives, nothing sleeps.
constexpr int kTimeoutMs = 10000;
constexpr int kFastDebounceMs = 10;

bool waitFor(QSignalSpy& spy)
{
    return !spy.isEmpty() || spy.wait(kTimeoutMs);
}

// Collects every path of all emissions of a QStringList signal.
QStringList allPaths(const QSignalSpy& spy)
{
    QStringList out;
    for (const QList<QVariant>& args : spy) {
        out << args.at(0).toStringList();
    }
    out.sort();
    return out;
}

// gtest fixture idiom: tests read the members directly.
// NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
class VaultWatcherTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        m_root = m_dir.path();
    }

    // Opens the vault and starts watching; returns once the watcher has registered all of its
    // watches and finished its first verification scan.
    void start(int debounceMs = kFastDebounceMs)
    {
        auto vault = Vault::open(m_root);
        ASSERT_TRUE(vault.has_value());
        m_watcher = std::make_unique<VaultWatcher>(*vault);
        m_watcher->setDebounceMs(debounceMs);
        m_added = std::make_unique<QSignalSpy>(m_watcher.get(), &VaultWatcher::notesAdded);
        m_removed = std::make_unique<QSignalSpy>(m_watcher.get(), &VaultWatcher::notesRemoved);
        m_modified = std::make_unique<QSignalSpy>(m_watcher.get(), &VaultWatcher::notesModified);
        m_folders = std::make_unique<QSignalSpy>(m_watcher.get(), &VaultWatcher::foldersChanged);
        m_rescanned = std::make_unique<QSignalSpy>(m_watcher.get(), &VaultWatcher::rescanned);
        ASSERT_TRUE(waitFor(*m_rescanned));
        clearSpies();
    }

    void clearSpies()
    {
        m_added->clear();
        m_removed->clear();
        m_modified->clear();
        m_folders->clear();
        m_rescanned->clear();
    }

    // Forces one scan that starts after this call and waits for it.
    void settle()
    {
        m_rescanned->clear();
        m_watcher->rescanNow();
        ASSERT_TRUE(waitFor(*m_rescanned));
    }

    QTemporaryDir m_dir;
    QString m_root;
    std::unique_ptr<VaultWatcher> m_watcher;
    std::unique_ptr<QSignalSpy> m_added;
    std::unique_ptr<QSignalSpy> m_removed;
    std::unique_ptr<QSignalSpy> m_modified;
    std::unique_ptr<QSignalSpy> m_folders;
    std::unique_ptr<QSignalSpy> m_rescanned;
};
// NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

} // namespace

TEST_F(VaultWatcherTest, DefaultDebounceIs150Ms)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "x"));
    auto vault = Vault::open(m_root);
    ASSERT_TRUE(vault.has_value());
    const VaultWatcher watcher(*vault);
    EXPECT_EQ(watcher.debounceMs(), 150);
}

TEST_F(VaultWatcherTest, DetectsExternallyAddedNote)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "x"));
    start();
    ASSERT_TRUE(writeFile(m_root, "b.md", "new"));
    ASSERT_TRUE(waitFor(*m_added));
    EXPECT_EQ(allPaths(*m_added), (QStringList{"b.md"}));
    EXPECT_TRUE(m_removed->isEmpty());
    EXPECT_TRUE(m_modified->isEmpty());
}

TEST_F(VaultWatcherTest, DetectsExternallyModifiedNote)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "short"));
    ASSERT_TRUE(writeFile(m_root, "other.md", "same"));
    start();
    ASSERT_TRUE(writeFile(m_root, "a.md", "a much longer replacement"));
    ASSERT_TRUE(waitFor(*m_modified));
    EXPECT_EQ(allPaths(*m_modified), (QStringList{"a.md"}));
    EXPECT_TRUE(m_added->isEmpty());
    EXPECT_TRUE(m_removed->isEmpty());
}

TEST_F(VaultWatcherTest, DetectsExternallyRemovedNote)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "x"));
    ASSERT_TRUE(writeFile(m_root, "b.md", "x"));
    start();
    ASSERT_TRUE(QFile::remove(m_root + "/a.md"));
    ASSERT_TRUE(waitFor(*m_removed));
    EXPECT_EQ(allPaths(*m_removed), (QStringList{"a.md"}));
    EXPECT_TRUE(m_added->isEmpty());
}

TEST_F(VaultWatcherTest, RenameIsOneRemovedPlusOneAdded)
{
    ASSERT_TRUE(writeFile(m_root, "old.md", "x"));
    start();
    ASSERT_TRUE(QFile::rename(m_root + "/old.md", m_root + "/new.md"));
    ASSERT_TRUE(waitFor(*m_added));
    ASSERT_TRUE(waitFor(*m_removed));
    EXPECT_EQ(allPaths(*m_removed), (QStringList{"old.md"}));
    EXPECT_EQ(allPaths(*m_added), (QStringList{"new.md"}));
}

TEST_F(VaultWatcherTest, DetectsNewFolderAndThenNoteInsideIt)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "x"));
    start();
    ASSERT_TRUE(makeDir(m_root, "fresh"));
    ASSERT_TRUE(waitFor(*m_folders));
    EXPECT_TRUE(m_added->isEmpty());

    // The folder is only watched after the re-scan; a note created afterwards must be seen.
    settle();
    m_added->clear();
    ASSERT_TRUE(writeFile(m_root, "fresh/inner.md", "hello"));
    ASSERT_TRUE(waitFor(*m_added));
    EXPECT_EQ(allPaths(*m_added), (QStringList{"fresh/inner.md"}));
}

TEST_F(VaultWatcherTest, DetectsNoteInFolderCreatedTogetherWithIt)
{
    start();
    ASSERT_TRUE(writeFile(m_root, "deep/er/note.md", "hello"));
    ASSERT_TRUE(waitFor(*m_added));
    EXPECT_EQ(allPaths(*m_added), (QStringList{"deep/er/note.md"}));
    EXPECT_FALSE(m_folders->isEmpty());
}

TEST_F(VaultWatcherTest, DetectsEditInsideNewlyWatchedFolder)
{
    ASSERT_TRUE(writeFile(m_root, "sub/n.md", "one"));
    start();
    ASSERT_TRUE(writeFile(m_root, "sub/n.md", "changed content"));
    ASSERT_TRUE(waitFor(*m_modified));
    EXPECT_EQ(allPaths(*m_modified), (QStringList{"sub/n.md"}));
}

TEST_F(VaultWatcherTest, BatchOfChangesIsOneSignal)
{
    start(/*debounceMs=*/500);
    for (const char* name : {"1.md", "2.md", "3.md", "4.md", "5.md"}) {
        ASSERT_TRUE(writeFile(m_root, name, "x"));
    }
    ASSERT_TRUE(waitFor(*m_added));
    EXPECT_EQ(m_added->count(), 1);
    EXPECT_EQ(allPaths(*m_added), (QStringList{"1.md", "2.md", "3.md", "4.md", "5.md"}));
}

TEST_F(VaultWatcherTest, OwnWritesDoNotTriggerNotesModified)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "original"));
    start();

    NoteFiles files(m_root);
    files.setWriteListener(m_watcher->writeListener());
    const NoteDocument doc{QStringLiteral("rewritten by nexus, different length"), LineEnding::Lf,
                           false};
    ASSERT_TRUE(files.write("a.md", doc).has_value());
    settle();
    EXPECT_TRUE(m_modified->isEmpty());
    EXPECT_TRUE(m_added->isEmpty());

    // The same file changed from outside afterwards is still reported.
    ASSERT_TRUE(writeFile(m_root, "a.md", "edited elsewhere with yet another length"));
    ASSERT_TRUE(waitFor(*m_modified));
    EXPECT_EQ(allPaths(*m_modified), (QStringList{"a.md"}));
}

TEST_F(VaultWatcherTest, ExternalEditAfterOwnWriteIsNotSwallowed)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "original"));
    start();

    NoteFiles files(m_root);
    files.setWriteListener(m_watcher->writeListener());
    const NoteDocument doc{QStringLiteral("ours"), LineEnding::Lf, false};
    ASSERT_TRUE(files.write("a.md", doc).has_value());
    // Another program overwrites before the watcher ever scanned our write.
    ASSERT_TRUE(writeFile(m_root, "a.md", "theirs, noticeably longer than ours"));
    settle();
    EXPECT_EQ(allPaths(*m_modified), (QStringList{"a.md"}));
}

TEST_F(VaultWatcherTest, IgnoresHiddenEntries)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "x"));
    start();
    ASSERT_TRUE(writeFile(m_root, ".hidden.md", "x"));
    ASSERT_TRUE(writeFile(m_root, ".git/inner.md", "x"));
    settle();
    EXPECT_TRUE(m_added->isEmpty());
    EXPECT_TRUE(m_folders->isEmpty());
}

TEST_F(VaultWatcherTest, DestroyingWhileScanningIsSafe)
{
    for (int i = 0; i < 50; ++i) {
        ASSERT_TRUE(writeFile(m_root, QString("n%1.md").arg(i), "x"));
    }
    auto vault = Vault::open(m_root);
    ASSERT_TRUE(vault.has_value());
    {
        VaultWatcher watcher(*vault);
        watcher.rescanNow();
    }
    SUCCEED();
}
