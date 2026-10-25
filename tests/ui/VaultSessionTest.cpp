#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

#include "TestSupport.h"
#include "core/NoteDocument.h"
#include "ui/VaultSession.h"

using nexus::test::waitFor;
using nexus::test::writeFile;
using nexus::ui::VaultOpenError;
using nexus::ui::VaultSession;

namespace {

// Collects the session signals so tests can assert on what happened and in which order.
struct Recorder {
    explicit Recorder(VaultSession& session)
    {
        QObject::connect(&session, &VaultSession::opening, &session, [this](const QString& p) {
            events << QStringLiteral("opening");
            lastPath = p;
        });
        QObject::connect(&session, &VaultSession::opened, &session,
                         [this] { events << QStringLiteral("opened"); });
        QObject::connect(&session, &VaultSession::closed, &session,
                         [this] { events << QStringLiteral("closed"); });
        QObject::connect(&session, &VaultSession::openFailed, &session,
                         [this](const QString&, const VaultOpenError& e) {
                             events << QStringLiteral("failed");
                             error = e;
                         });
    }

    [[nodiscard]] int count(const QString& name) const
    {
        return static_cast<int>(events.count(name));
    }

    QStringList events;
    QString lastPath;
    std::optional<VaultOpenError> error;
};

class VaultSessionTest : public ::testing::Test {
protected:
    [[nodiscard]] QString path(const QString& rel) const { return m_dir.filePath(rel); }
    void makeNotes()
    {
        ASSERT_TRUE(writeFile(path(QStringLiteral("v/a.md"))));
        ASSERT_TRUE(writeFile(path(QStringLiteral("v/sub/b.md"))));
        ASSERT_TRUE(writeFile(path(QStringLiteral("v/readme.txt"))));
    }

private:
    QTemporaryDir m_dir;
};

} // namespace

TEST_F(VaultSessionTest, OpensAVaultInTheBackground)
{
    makeNotes();
    VaultSession session;
    Recorder rec(session);

    session.open(path(QStringLiteral("v")));
    // Returns immediately: nothing has been read on this thread.
    EXPECT_EQ(session.state(), VaultSession::State::Opening);
    EXPECT_EQ(rec.events, QStringList{QStringLiteral("opening")});

    ASSERT_TRUE(waitFor([&] { return session.isOpen(); }));
    EXPECT_EQ(rec.count(QStringLiteral("opened")), 1);
    EXPECT_EQ(session.name(), QStringLiteral("v"));
    EXPECT_EQ(session.noteCount(), 2);
    EXPECT_EQ(session.folderCount(), 1);
    EXPECT_NE(session.files(), nullptr);
    EXPECT_NE(session.watcher(), nullptr);
}

TEST_F(VaultSessionTest, MissingFolderFails)
{
    VaultSession session;
    Recorder rec(session);
    session.open(path(QStringLiteral("nope")));
    ASSERT_TRUE(waitFor([&] { return rec.count(QStringLiteral("failed")) == 1; }));
    EXPECT_EQ(session.state(), VaultSession::State::Closed);
    ASSERT_TRUE(rec.error.has_value());
    EXPECT_EQ(rec.error->file.kind, nexus::core::FileErrorKind::NotFound);
    EXPECT_FALSE(rec.error->folderNotEmpty);
    EXPECT_EQ(session.files(), nullptr);
}

TEST_F(VaultSessionTest, FileInsteadOfFolderFails)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("file.md"))));
    VaultSession session;
    Recorder rec(session);
    session.open(path(QStringLiteral("file.md")));
    ASSERT_TRUE(waitFor([&] { return rec.count(QStringLiteral("failed")) == 1; }));
    EXPECT_EQ(session.state(), VaultSession::State::Closed);
}

TEST_F(VaultSessionTest, CreateWritesWelcomeNoteIntoEmptyFolder)
{
    ASSERT_TRUE(QDir().mkpath(path(QStringLiteral("fresh"))));
    VaultSession session;
    Recorder rec(session);
    session.createAndOpen(path(QStringLiteral("fresh")));
    ASSERT_TRUE(waitFor([&] { return session.isOpen(); }));

    EXPECT_EQ(session.noteCount(), 1);
    const auto doc = session.files()->read(QStringLiteral("歡迎.md"));
    ASSERT_TRUE(doc.has_value());
    EXPECT_EQ(doc->text, nexus::ui::welcomeNoteText());
    EXPECT_TRUE(doc->text.startsWith(QStringLiteral("# 歡迎使用 Nexus")));
}

TEST_F(VaultSessionTest, CreateRefusesAFolderWithContent)
{
    makeNotes();
    VaultSession session;
    Recorder rec(session);
    session.createAndOpen(path(QStringLiteral("v")));
    ASSERT_TRUE(waitFor([&] { return rec.count(QStringLiteral("failed")) == 1; }));
    ASSERT_TRUE(rec.error.has_value());
    EXPECT_TRUE(rec.error->folderNotEmpty);
    EXPECT_FALSE(QFile::exists(path(QStringLiteral("v/歡迎.md")))) << "must not touch the folder";
}

TEST_F(VaultSessionTest, CreateTreatsHiddenFilesAsContent)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("h/.git/config"))));
    VaultSession session;
    Recorder rec(session);
    session.createAndOpen(path(QStringLiteral("h")));
    ASSERT_TRUE(waitFor([&] { return rec.count(QStringLiteral("failed")) == 1; }));
    EXPECT_TRUE(rec.error->folderNotEmpty);
}

TEST_F(VaultSessionTest, CreateInMissingFolderFails)
{
    VaultSession session;
    Recorder rec(session);
    session.createAndOpen(path(QStringLiteral("missing")));
    ASSERT_TRUE(waitFor([&] { return rec.count(QStringLiteral("failed")) == 1; }));
    EXPECT_FALSE(rec.error->folderNotEmpty);
    EXPECT_EQ(rec.error->file.kind, nexus::core::FileErrorKind::NotFound);
}

TEST_F(VaultSessionTest, CloseReleasesEverything)
{
    makeNotes();
    VaultSession session;
    Recorder rec(session);
    session.open(path(QStringLiteral("v")));
    ASSERT_TRUE(waitFor([&] { return session.isOpen(); }));

    session.close();
    EXPECT_EQ(session.state(), VaultSession::State::Closed);
    EXPECT_EQ(rec.count(QStringLiteral("closed")), 1);
    EXPECT_EQ(session.files(), nullptr);
    EXPECT_EQ(session.watcher(), nullptr);
    EXPECT_EQ(session.noteCount(), 0);
    EXPECT_TRUE(session.root().isEmpty());

    session.close(); // already closed: no second signal
    EXPECT_EQ(rec.count(QStringLiteral("closed")), 1);
}

TEST_F(VaultSessionTest, OpeningAnotherVaultReplacesTheFirst)
{
    makeNotes();
    ASSERT_TRUE(writeFile(path(QStringLiteral("w/only.md"))));
    VaultSession session;
    Recorder rec(session);
    session.open(path(QStringLiteral("v")));
    ASSERT_TRUE(waitFor([&] { return session.isOpen(); }));

    session.open(path(QStringLiteral("w")));
    EXPECT_EQ(rec.count(QStringLiteral("closed")), 1);
    ASSERT_TRUE(waitFor([&] { return session.isOpen(); }));
    EXPECT_EQ(session.name(), QStringLiteral("w"));
    EXPECT_EQ(session.noteCount(), 1);
}

TEST_F(VaultSessionTest, ASupersededOpenIsIgnored)
{
    makeNotes();
    ASSERT_TRUE(writeFile(path(QStringLiteral("w/only.md"))));
    VaultSession session;
    Recorder rec(session);
    session.open(path(QStringLiteral("v")));
    session.open(path(QStringLiteral("w"))); // before the first answer arrived
    ASSERT_TRUE(waitFor([&] { return session.isOpen(); }));
    QThread::msleep(100);
    QCoreApplication::processEvents();
    EXPECT_EQ(rec.count(QStringLiteral("opened")), 1);
    EXPECT_EQ(session.name(), QStringLiteral("w"));
}

TEST_F(VaultSessionTest, CloseWhileOpeningDiscardsTheResult)
{
    makeNotes();
    VaultSession session;
    Recorder rec(session);
    session.open(path(QStringLiteral("v")));
    session.close();
    QThread::msleep(200);
    QCoreApplication::processEvents();
    EXPECT_EQ(rec.count(QStringLiteral("opened")), 0);
    EXPECT_EQ(session.state(), VaultSession::State::Closed);
    EXPECT_EQ(session.watcher(), nullptr);
}

TEST_F(VaultSessionTest, WatcherOutlivesABackgroundWriteStartedBeforeClose)
{
    makeNotes();
    VaultSession session;
    session.open(path(QStringLiteral("v")));
    ASSERT_TRUE(waitFor([&] { return session.isOpen(); }));

    // A task that keeps writing for a while (as the editor auto-save will).
    auto files = session.files();
    QFutureWatcher<bool> watcher;
    watcher.setFuture(QtConcurrent::run([files] {
        bool ok = true;
        for (int i = 0; i < 20; ++i) {
            nexus::core::NoteDocument doc;
            doc.text = QStringLiteral("round %1\n").arg(i);
            ok = ok && files->write(QStringLiteral("a.md"), doc).has_value();
            QThread::msleep(10);
        }
        return ok;
    }));
    // Drop our own reference like the UI does; the task still holds one.
    session.close();
    EXPECT_EQ(session.parkedCount(), 1) << "watcher must be kept while the write runs";

    ASSERT_TRUE(waitFor([&] { return watcher.isFinished(); }));
    EXPECT_TRUE(watcher.result());
    // `files` above is still ours; release it so the parked watcher can go.
    files.reset();
    ASSERT_TRUE(waitFor([&] { return session.parkedCount() == 0; }));
}

TEST_F(VaultSessionTest, NoteCountFollowsExternalChanges)
{
    makeNotes();
    VaultSession session;
    session.open(path(QStringLiteral("v")));
    ASSERT_TRUE(waitFor([&] { return session.isOpen(); }));

    int last = -1;
    QObject::connect(&session, &VaultSession::noteCountChanged, &session,
                     [&last](int c) { last = c; });
    ASSERT_TRUE(writeFile(path(QStringLiteral("v/new.md"))));
    session.watcher()->rescanNow();
    ASSERT_TRUE(waitFor([&] { return last == 3; }));
    EXPECT_EQ(session.noteCount(), 3);

    QFile::remove(path(QStringLiteral("v/new.md")));
    QFile::remove(path(QStringLiteral("v/a.md")));
    session.watcher()->rescanNow();
    ASSERT_TRUE(waitFor([&] { return last == 1; }));
}
