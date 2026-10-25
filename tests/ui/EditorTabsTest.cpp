#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include <QElapsedTimer>
#include <QFile>
#include <QSettings>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTextCursor>
#include <QTextDocument>

#include "TestSupport.h"
#include "core/NoteFiles.h"
#include "ui/EditorStatusWidget.h"
#include "ui/EditorTabs.h"
#include "ui/NoteEditor.h"
#include "ui/NoteSession.h"
#include "ui/ThemeManager.h"

using nexus::core::NoteFiles;
using nexus::test::waitFor;
using nexus::test::writeFile;
using nexus::ui::EditorTabs;
using nexus::ui::NoteSession;

namespace {

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
class EditorTabsTest : public ::testing::Test {
protected:
    EditorTabsTest()
        : m_settings(m_dir.filePath(QStringLiteral("s.ini")), QSettings::IniFormat),
          m_theme(m_settings)
    {
    }

    void SetUp() override
    {
        ASSERT_TRUE(writeFile(path(QStringLiteral("v/a.md")), "# A\nalpha\n"));
        ASSERT_TRUE(writeFile(path(QStringLiteral("v/sub/b.md")), "bravo 中文\n"));
        m_tabs.setVault(std::make_shared<const NoteFiles>(path(QStringLiteral("v"))),
                        QStringLiteral("v"));
        m_tabs.resize(900, 600);
        m_tabs.show();
    }

    [[nodiscard]] QString path(const QString& rel) const { return m_dir.filePath(rel); }

    bool openAndWait(const QString& rel, bool newTab)
    {
        m_tabs.openNote(rel, newTab);
        NoteSession* session = m_tabs.sessionFor(rel);
        return session != nullptr &&
               waitFor([session] { return session->state() == NoteSession::State::Ready; });
    }

    static void type(NoteSession& session, const QString& text)
    {
        QTextCursor cursor(session.document());
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(text);
    }

    QTemporaryDir m_dir;
    QSettings m_settings;
    nexus::ui::ThemeManager m_theme;
    EditorTabs m_tabs{m_theme};
};
// NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

} // namespace

TEST_F(EditorTabsTest, OpensNotesInTabsAndShowsBreadcrumb)
{
    ASSERT_TRUE(openAndWait(QStringLiteral("a.md"), true));
    ASSERT_TRUE(openAndWait(QStringLiteral("sub/b.md"), true));
    EXPECT_EQ(m_tabs.count(), 2);
    EXPECT_EQ(m_tabs.currentSession()->relPath(), QStringLiteral("sub/b.md"));
    EXPECT_EQ(m_tabs.breadcrumbText(), QStringLiteral("v › sub › b.md"));
    EXPECT_EQ(m_tabs.currentEditor()->toPlainText(), QStringLiteral("bravo 中文\n"));

    // Opening an open note switches to it instead of adding a tab.
    m_tabs.openNote(QStringLiteral("a.md"));
    EXPECT_EQ(m_tabs.count(), 2);
    EXPECT_EQ(m_tabs.currentSession()->relPath(), QStringLiteral("a.md"));
}

TEST_F(EditorTabsTest, OpenWithoutNewTabReplacesTheCurrentTab)
{
    ASSERT_TRUE(openAndWait(QStringLiteral("a.md"), true));
    type(*m_tabs.currentSession(), QStringLiteral("more"));
    ASSERT_TRUE(openAndWait(QStringLiteral("sub/b.md"), false));
    EXPECT_EQ(m_tabs.count(), 1);
    // The replaced tab was saved first.
    EXPECT_EQ(readAll(path(QStringLiteral("v/a.md"))), "# A\nalpha\nmore");
}

TEST_F(EditorTabsTest, ClosingSavesAndReportsClean)
{
    ASSERT_TRUE(openAndWait(QStringLiteral("a.md"), true));
    QStringList dirtyEvents;
    QObject::connect(&m_tabs, &EditorTabs::dirtyChanged, &m_tabs,
                     [&](const QString& rel, bool dirty) {
                         dirtyEvents << QStringLiteral("%1=%2").arg(rel).arg(dirty);
                     });
    type(*m_tabs.currentSession(), QStringLiteral("!"));
    EXPECT_EQ(dirtyEvents.value(0), QStringLiteral("a.md=1"));
    m_tabs.closeCurrentTab();
    EXPECT_EQ(m_tabs.count(), 0);
    EXPECT_EQ(dirtyEvents.last(), QStringLiteral("a.md=0"));
    EXPECT_EQ(readAll(path(QStringLiteral("v/a.md"))), "# A\nalpha\n!");
}

TEST_F(EditorTabsTest, SwitchingTabsSavesTheTabLeft)
{
    ASSERT_TRUE(openAndWait(QStringLiteral("a.md"), true));
    NoteSession* a = m_tabs.currentSession();
    type(*a, QStringLiteral("x"));
    ASSERT_TRUE(openAndWait(QStringLiteral("sub/b.md"), true));
    ASSERT_TRUE(waitFor([a] { return !a->isDirty(); }));
    EXPECT_EQ(readAll(path(QStringLiteral("v/a.md"))), "# A\nalpha\nx");
}

TEST_F(EditorTabsTest, RenameAndRemoveFollowTheFileTree)
{
    ASSERT_TRUE(openAndWait(QStringLiteral("a.md"), true));
    ASSERT_TRUE(openAndWait(QStringLiteral("sub/b.md"), true));

    m_tabs.renameNote(QStringLiteral("sub"), QStringLiteral("docs"));
    EXPECT_NE(m_tabs.sessionFor(QStringLiteral("docs/b.md")), nullptr);
    EXPECT_EQ(m_tabs.breadcrumbText(), QStringLiteral("v › docs › b.md"));

    m_tabs.removeNote(QStringLiteral("a.md"));
    EXPECT_EQ(m_tabs.count(), 1);
    EXPECT_EQ(m_tabs.sessionFor(QStringLiteral("a.md")), nullptr);
}

TEST_F(EditorTabsTest, StatusShowsSaveStateCursorWordsAndLineEnding)
{
    nexus::ui::EditorStatusWidget status(m_tabs);
    status.show();
    ASSERT_TRUE(openAndWait(QStringLiteral("sub/b.md"), true));
    ASSERT_TRUE(waitFor([&] { return status.summary().contains(QStringLiteral("3 字")); }));
    EXPECT_EQ(status.summary(), QStringLiteral("已儲存 · 第 1 行，第 1 欄 · 3 字 · UTF-8 · LF"));

    type(*m_tabs.currentSession(), QStringLiteral("x"));
    m_tabs.currentEditor()->moveCursor(QTextCursor::End);
    EXPECT_TRUE(status.summary().startsWith(QStringLiteral("未儲存 · 第 2 行，第 2 欄")))
        << status.summary().toStdString();
}

TEST_F(EditorTabsTest, OpensALargeNoteQuickly)
{
    QByteArray big;
    for (int i = 0; i < 10000; ++i) {
        big += "- line " + QByteArray::number(i) + " with [[link]] and #tag and `code`\n";
    }
    ASSERT_TRUE(writeFile(path(QStringLiteral("v/big.md")), big));
    QElapsedTimer timer;
    timer.start();
    ASSERT_TRUE(openAndWait(QStringLiteral("big.md"), true));
    const qint64 elapsed = timer.elapsed();
    EXPECT_EQ(m_tabs.currentSession()->document()->blockCount(), 10001);
    // Budget is 300 ms in release builds (TASKS.md T-113); debug and sanitizer builds are
    // several times slower, so only catch gross regressions here.
    EXPECT_LT(elapsed, 5000) << "opening 10k lines took " << elapsed << " ms";
    std::cout << "[ perf ] open 10k lines: " << elapsed << " ms\n";
}
