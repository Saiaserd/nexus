#include <gtest/gtest.h>

#include <memory>

#include <QFile>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>

#include "TestSupport.h"
#include "core/NoteFiles.h"
#include "ui/NoteSession.h"

using nexus::core::LineEnding;
using nexus::core::NoteFiles;
using nexus::test::waitFor;
using nexus::test::writeFile;
using nexus::ui::NoteSession;

namespace {

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void typeAtEnd(NoteSession& session, const QString& text)
{
    QTextCursor cursor(session.document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
}

// NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
class NoteSessionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        m_files = std::make_shared<const NoteFiles>(m_dir.path());
    }

    [[nodiscard]] QString path(const QString& rel) const { return m_dir.filePath(rel); }

    std::unique_ptr<NoteSession> openLoaded(const QString& rel)
    {
        auto session = std::make_unique<NoteSession>(m_files, rel);
        session->load();
        EXPECT_TRUE(waitFor([&] { return session->state() != NoteSession::State::Loading; }));
        return session;
    }

    QTemporaryDir m_dir;
    std::shared_ptr<const NoteFiles> m_files;
};
// NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

} // namespace

TEST_F(NoteSessionTest, LoadsTextAndIsClean)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("a.md")), "# A\nbody\n"));
    const auto session = openLoaded(QStringLiteral("a.md"));
    ASSERT_EQ(session->state(), NoteSession::State::Ready);
    EXPECT_EQ(session->document()->toPlainText(), QStringLiteral("# A\nbody\n"));
    EXPECT_FALSE(session->isDirty());
    EXPECT_EQ(session->saveState(), NoteSession::SaveState::Saved);
    EXPECT_FALSE(session->document()->isUndoAvailable()); // loading is not an undo step
}

TEST_F(NoteSessionTest, MissingFileFails)
{
    const auto session = openLoaded(QStringLiteral("nope.md"));
    EXPECT_EQ(session->state(), NoteSession::State::Failed);
}

TEST_F(NoteSessionTest, EditMarksDirtyAndAutosaves)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("a.md")), "x\n"));
    const auto session = openLoaded(QStringLiteral("a.md"));
    typeAtEnd(*session, QStringLiteral("y"));
    EXPECT_TRUE(session->isDirty());
    EXPECT_EQ(session->saveState(), NoteSession::SaveState::Unsaved);
    EXPECT_EQ(readAll(path(QStringLiteral("a.md"))), "x\n"); // not before the idle delay

    ASSERT_TRUE(waitFor([&] { return !session->isDirty(); }, 5000));
    EXPECT_EQ(readAll(path(QStringLiteral("a.md"))), "x\ny");
    EXPECT_EQ(session->saveState(), NoteSession::SaveState::Saved);
}

TEST_F(NoteSessionTest, SavePreservesCrlfAndBom)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("w.md")), "\xEF\xBB\xBFone\r\ntwo\r\n"));
    const auto session = openLoaded(QStringLiteral("w.md"));
    EXPECT_EQ(session->lineEnding(), LineEnding::Crlf);
    EXPECT_TRUE(session->hasBom());
    typeAtEnd(*session, QStringLiteral("three\n"));
    session->save();
    ASSERT_TRUE(waitFor([&] { return !session->isDirty(); }));
    EXPECT_EQ(readAll(path(QStringLiteral("w.md"))), "\xEF\xBB\xBFone\r\ntwo\r\nthree\r\n");
}

TEST_F(NoteSessionTest, EditsDuringAWriteAreSavedAfterIt)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("a.md")), ""));
    const auto session = openLoaded(QStringLiteral("a.md"));
    typeAtEnd(*session, QStringLiteral("1"));
    session->save();
    typeAtEnd(*session, QStringLiteral("2")); // while the first write may still run
    session->save();
    ASSERT_TRUE(waitFor([&] { return !session->isDirty(); }, 5000));
    EXPECT_EQ(readAll(path(QStringLiteral("a.md"))), "12");
}

TEST_F(NoteSessionTest, SaveAndWaitWritesSynchronously)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("a.md")), "a"));
    const auto session = openLoaded(QStringLiteral("a.md"));
    typeAtEnd(*session, QStringLiteral("b"));
    EXPECT_TRUE(session->saveAndWait());
    EXPECT_FALSE(session->isDirty());
    EXPECT_EQ(readAll(path(QStringLiteral("a.md"))), "ab");
}

TEST_F(NoteSessionTest, RenameWritesToTheNewPath)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("a.md")), "a"));
    const auto session = openLoaded(QStringLiteral("a.md"));
    ASSERT_TRUE(m_files->rename(QStringLiteral("a.md"), QStringLiteral("b")).has_value());
    session->setRelPath(QStringLiteral("b.md"));
    typeAtEnd(*session, QStringLiteral("!"));
    EXPECT_TRUE(session->saveAndWait());
    EXPECT_EQ(readAll(path(QStringLiteral("b.md"))), "a!");
    EXPECT_FALSE(QFile::exists(path(QStringLiteral("a.md"))));
}

TEST_F(NoteSessionTest, DestroyingWithAWriteInFlightCompletesIt)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("a.md")), ""));
    auto session = openLoaded(QStringLiteral("a.md"));
    typeAtEnd(*session, QStringLiteral("z"));
    session->save();
    session.reset();
    EXPECT_EQ(readAll(path(QStringLiteral("a.md"))), "z");
}
