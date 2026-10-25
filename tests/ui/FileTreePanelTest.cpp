#include <gtest/gtest.h>

#include <memory>

#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>

#include "TestSupport.h"
#include "ui/FileTreePanel.h"
#include "ui/ThemeManager.h"
#include "ui/VaultSession.h"
#include "ui/VaultSidebar.h"
#include "ui/VaultTreeModel.h"

using nexus::test::waitFor;
using nexus::test::writeFile;
using nexus::ui::VaultSession;
using nexus::ui::VaultSidebar;
using nexus::ui::VaultTreeModel;

namespace {

class FileTreePanelTest : public ::testing::Test {
protected:
    FileTreePanelTest()
        : m_settings(m_dir.filePath(QStringLiteral("t.ini")), QSettings::IniFormat),
          m_theme(m_settings)
    {
    }

    void SetUp() override
    {
        ASSERT_TRUE(writeFile(path(QStringLiteral("v/a.md"))));
        ASSERT_TRUE(writeFile(path(QStringLiteral("v/sub/b.md"))));
        m_sidebar = std::make_unique<VaultSidebar>(m_theme, &m_settings);
        m_sidebar->setSession(m_session);
        m_sidebar->setVault(QStringLiteral("v"), 2);
        m_sidebar->resize(260, 500);
        m_sidebar->show();
        m_session.open(path(QStringLiteral("v")));
        ASSERT_TRUE(waitFor([this] { return m_session.isOpen(); }));
    }

    void TearDown() override
    {
        m_sidebar.reset();
        m_session.close();
    }

    [[nodiscard]] QString path(const QString& rel) const { return m_dir.filePath(rel); }
    [[nodiscard]] VaultTreeModel* model() const { return m_sidebar->panel()->model(); }
    [[nodiscard]] QTreeView* view() const { return m_sidebar->panel()->view(); }
    [[nodiscard]] QLineEdit* editor() const { return view()->findChild<QLineEdit*>(); }

    QTemporaryDir m_dir;
    QSettings m_settings;
    nexus::ui::ThemeManager m_theme;
    VaultSession m_session;
    std::unique_ptr<VaultSidebar> m_sidebar;
};

} // namespace

TEST_F(FileTreePanelTest, ShowsTheOpenedVault)
{
    EXPECT_TRUE(model()->contains(QStringLiteral("a.md")));
    EXPECT_TRUE(model()->contains(QStringLiteral("sub/b.md")));
    EXPECT_EQ(m_sidebar->vaultName(), QStringLiteral("v"));
}

TEST_F(FileTreePanelTest, ClickingANoteOpensItAndAFolderToggles)
{
    QSignalSpy spy(m_sidebar.get(), &VaultSidebar::openNoteRequested);
    const QModelIndex note = model()->indexForPath(QStringLiteral("a.md"));
    QTest::mouseClick(view()->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view()->visualRect(note).center());
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("a.md"));
    EXPECT_FALSE(spy.at(0).at(1).toBool());

    const QModelIndex folder = model()->indexForPath(QStringLiteral("sub"));
    EXPECT_FALSE(view()->isExpanded(folder));
    QTest::mouseClick(view()->viewport(), Qt::LeftButton, Qt::NoModifier,
                      view()->visualRect(folder).center());
    EXPECT_TRUE(view()->isExpanded(folder));
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(FileTreePanelTest, NewNoteCreatesTheFileOpensItAndStartsRenaming)
{
    QSignalSpy opened(m_sidebar.get(), &VaultSidebar::openNoteRequested);
    m_sidebar->newNote();
    ASSERT_TRUE(waitFor([&] { return opened.count() == 1; }));
    const QString rel = opened.at(0).at(0).toString();
    EXPECT_EQ(rel, QStringLiteral("未命名.md"));
    EXPECT_TRUE(QFileInfo::exists(path(QStringLiteral("v/") + rel)));
    EXPECT_TRUE(model()->contains(rel));
    ASSERT_NE(editor(), nullptr);
    EXPECT_EQ(editor()->text(), QStringLiteral("未命名"));
}

TEST_F(FileTreePanelTest, NewNoteGoesIntoTheFolderOfTheSelection)
{
    m_sidebar->selectNote(QStringLiteral("sub/b.md"));
    EXPECT_TRUE(view()->isExpanded(model()->indexForPath(QStringLiteral("sub"))));
    QSignalSpy opened(m_sidebar.get(), &VaultSidebar::openNoteRequested);
    m_sidebar->newNote();
    ASSERT_TRUE(waitFor([&] { return opened.count() == 1; }));
    EXPECT_EQ(opened.at(0).at(0).toString(), QStringLiteral("sub/未命名.md"));
}

TEST_F(FileTreePanelTest, RenamingWithEnterRenamesTheFileAndAnnouncesIt)
{
    QSignalSpy renamed(m_sidebar.get(), &VaultSidebar::noteRenamed);
    m_sidebar->selectNote(QStringLiteral("a.md"));
    view()->setFocus();
    QTest::keyClick(view(), Qt::Key_F2);
    ASSERT_NE(editor(), nullptr);
    editor()->setText(QStringLiteral("renamed"));
    QTest::keyClick(editor(), Qt::Key_Return);
    ASSERT_TRUE(waitFor([&] { return renamed.count() == 1; }));
    EXPECT_EQ(renamed.at(0).at(0).toString(), QStringLiteral("a.md"));
    EXPECT_EQ(renamed.at(0).at(1).toString(), QStringLiteral("renamed.md"));
    EXPECT_TRUE(QFileInfo::exists(path(QStringLiteral("v/renamed.md"))));
    EXPECT_FALSE(QFileInfo::exists(path(QStringLiteral("v/a.md"))));
    EXPECT_TRUE(model()->contains(QStringLiteral("renamed.md")));
    EXPECT_FALSE(model()->contains(QStringLiteral("a.md")));
}

TEST_F(FileTreePanelTest, InvalidNameShowsTheReasonAndCannotBeConfirmed)
{
    ASSERT_TRUE(writeFile(path(QStringLiteral("v/taken.md"))));
    m_session.watcher()->rescanNow();
    ASSERT_TRUE(waitFor([&] { return model()->contains(QStringLiteral("taken.md")); }));

    QSignalSpy renamed(m_sidebar.get(), &VaultSidebar::noteRenamed);
    m_sidebar->selectNote(QStringLiteral("a.md"));
    view()->setFocus();
    QTest::keyClick(view(), Qt::Key_F2);
    ASSERT_NE(editor(), nullptr);

    editor()->setText(QStringLiteral("bad:name"));
    auto* error = view()->findChild<QLabel*>(QStringLiteral("renameError"));
    ASSERT_NE(error, nullptr);
    EXPECT_FALSE(error->isHidden());
    QTest::keyClick(editor(), Qt::Key_Return);
    ASSERT_NE(editor(), nullptr); // still editing
    EXPECT_TRUE(QFileInfo::exists(path(QStringLiteral("v/a.md"))));

    editor()->setText(QStringLiteral("fine"));
    EXPECT_TRUE(error->isHidden());

    editor()->setText(QStringLiteral("TAKEN")); // differs only by case: still a clash
    EXPECT_FALSE(error->isHidden());
    EXPECT_TRUE(error->text().contains(QStringLiteral("TAKEN")));
    EXPECT_EQ(renamed.count(), 0);
}

TEST_F(FileTreePanelTest, EscapeCancelsRenaming)
{
    m_sidebar->selectNote(QStringLiteral("a.md"));
    view()->setFocus();
    QTest::keyClick(view(), Qt::Key_F2);
    ASSERT_NE(editor(), nullptr);
    editor()->setText(QStringLiteral("other"));
    QTest::keyClick(editor(), Qt::Key_Escape);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(editor(), nullptr);
    EXPECT_TRUE(QFileInfo::exists(path(QStringLiteral("v/a.md"))));
}

TEST_F(FileTreePanelTest, ExternalChangesUpdateTheTree)
{
    QSignalSpy removed(m_sidebar.get(), &VaultSidebar::noteRemoved);
    ASSERT_TRUE(writeFile(path(QStringLiteral("v/new/c.md"))));
    m_session.watcher()->rescanNow();
    ASSERT_TRUE(waitFor([&] { return model()->contains(QStringLiteral("new/c.md")); }));

    ASSERT_TRUE(QFile::remove(path(QStringLiteral("v/a.md"))));
    m_session.watcher()->rescanNow();
    ASSERT_TRUE(waitFor([&] { return !model()->contains(QStringLiteral("a.md")); }));
    ASSERT_EQ(removed.count(), 1);
    EXPECT_EQ(removed.at(0).at(0).toString(), QStringLiteral("a.md"));
}

TEST_F(FileTreePanelTest, DirtyDotAndClosingTheVault)
{
    m_sidebar->setNoteDirty(QStringLiteral("a.md"), true);
    EXPECT_TRUE(
        model()->indexForPath(QStringLiteral("a.md")).data(VaultTreeModel::DirtyRole).toBool());
    m_session.close();
    EXPECT_EQ(model()->rowCount(), 0);
}

TEST_F(FileTreePanelTest, DeleteMovesToTrashWhenNotAskingForConfirmation)
{
    m_settings.setValue(QStringLiteral("fileTree/confirmDelete"), false);
    QSignalSpy removed(m_sidebar.get(), &VaultSidebar::noteRemoved);
    m_sidebar->selectNote(QStringLiteral("a.md"));
    view()->setFocus();
    QTest::keyClick(view(), Qt::Key_Delete);
    // No recycle bin (e.g. a headless CI box): the note must stay and nothing is announced.
    if (!waitFor([&] { return removed.count() == 1; }, 3000)) {
        EXPECT_TRUE(QFileInfo::exists(path(QStringLiteral("v/a.md"))));
        GTEST_SKIP() << "recycle bin not available";
    }
    EXPECT_FALSE(model()->contains(QStringLiteral("a.md")));
    EXPECT_FALSE(QFileInfo::exists(path(QStringLiteral("v/a.md"))));
}
