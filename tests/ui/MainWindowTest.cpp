#include <gtest/gtest.h>

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>

#include "TestSupport.h"
#include "ui/MainWindow.h"
#include "ui/RecentVaults.h"
#include "ui/ThemeManager.h"
#include "ui/VaultSidebar.h"
#include "ui/VaultStatusWidget.h"

using nexus::test::waitFor;
using nexus::test::writeFile;
using nexus::ui::MainWindow;

namespace {

class MainWindowTest : public ::testing::Test {
protected:
    MainWindowTest()
        : m_settings(m_dir.filePath(QStringLiteral("m.ini")), QSettings::IniFormat),
          m_theme(m_settings)
    {
    }

    [[nodiscard]] QString path(const QString& rel) const { return m_dir.filePath(rel); }

    [[nodiscard]] static QAction* findAction(const QMenu* menu, const QString& text)
    {
        for (QAction* action : menu->actions()) {
            if (action->text() == text) {
                return action;
            }
        }
        return nullptr;
    }

    [[nodiscard]] static QMenu* findMenu(const MainWindow& window, const QString& title)
    {
        for (QAction* action : window.menuBar()->actions()) {
            if (action->text() == title) {
                return action->menu();
            }
        }
        return nullptr;
    }

    void makeVault(const QString& name, int notes)
    {
        for (int i = 0; i < notes; ++i) {
            ASSERT_TRUE(writeFile(path(QStringLiteral("%1/n%2.md").arg(name).arg(i))));
        }
    }

    QTemporaryDir m_dir;
    QSettings m_settings;
    nexus::ui::ThemeManager m_theme;
};

} // namespace

TEST_F(MainWindowTest, StartsOnTheWelcomePage)
{
    MainWindow window(m_theme, m_settings);
    window.show();
    EXPECT_EQ(window.currentPage(), MainWindow::Page::Welcome);
    EXPECT_EQ(window.windowTitle(), QStringLiteral("Nexus"));
    EXPECT_FALSE(window.findChild<nexus::ui::VaultSidebar*>()->isVisible());
    EXPECT_EQ(window.findChild<nexus::ui::VaultStatusWidget*>()->summary(),
              QStringLiteral("尚未開啟 vault"));
}

TEST_F(MainWindowTest, MenusFollowTheSpecification)
{
    MainWindow window(m_theme, m_settings);
    QStringList titles;
    for (const QAction* action : window.menuBar()->actions()) {
        titles << action->text();
    }
    EXPECT_EQ(titles, (QStringList{QStringLiteral("檔案"), QStringLiteral("編輯"),
                                   QStringLiteral("檢視"), QStringLiteral("說明")}));

    const QMenu* file = findMenu(window, QStringLiteral("檔案"));
    ASSERT_NE(file, nullptr);
    QStringList fileItems;
    for (const QAction* action : file->actions()) {
        if (!action->isSeparator()) {
            fileItems << action->text();
        }
    }
    EXPECT_EQ(fileItems,
              (QStringList{QStringLiteral("開啟 vault…"), QStringLiteral("建立新 vault…"),
                           QStringLiteral("最近開啟"), QStringLiteral("新增筆記"),
                           QStringLiteral("儲存"), QStringLiteral("關閉分頁"),
                           QStringLiteral("關閉 vault"), QStringLiteral("結束")}));

    EXPECT_EQ(findAction(file, QStringLiteral("開啟 vault…"))->shortcut(),
              QKeySequence(QKeySequence::Open));
    EXPECT_EQ(findAction(file, QStringLiteral("新增筆記"))->shortcut(),
              QKeySequence(QKeySequence::New));
    EXPECT_EQ(findAction(file, QStringLiteral("儲存"))->shortcut(),
              QKeySequence(QKeySequence::Save));
    EXPECT_EQ(findAction(file, QStringLiteral("關閉分頁"))->shortcut(),
              QKeySequence(QKeySequence::Close));
    EXPECT_EQ(findAction(file, QStringLiteral("開啟 vault…"))->shortcut().toString(),
              QStringLiteral("Ctrl+O"));

    const QMenu* view = findMenu(window, QStringLiteral("檢視"));
    ASSERT_NE(view, nullptr);
    EXPECT_NE(findAction(view, QStringLiteral("主題")), nullptr);
    const QAction* sidebar = findAction(view, QStringLiteral("顯示檔案樹"));
    ASSERT_NE(sidebar, nullptr);
    EXPECT_EQ(sidebar->shortcut().toString(), QStringLiteral("Ctrl+B"));
    EXPECT_TRUE(sidebar->isCheckable());
    EXPECT_NE(findAction(view, QStringLiteral("顯示行號")), nullptr);

    const QMenu* help = findMenu(window, QStringLiteral("說明"));
    ASSERT_NE(help, nullptr);
    EXPECT_NE(findAction(help, QStringLiteral("關於 Nexus")), nullptr);
}

TEST_F(MainWindowTest, ThemeMenuHasThreeModesAndSwitches)
{
    MainWindow window(m_theme, m_settings);
    const QMenu* view = findMenu(window, QStringLiteral("檢視"));
    const QMenu* themes = findAction(view, QStringLiteral("主題"))->menu();
    ASSERT_EQ(themes->actions().size(), 3);
    EXPECT_TRUE(themes->actions().at(0)->isChecked()) << "default is light";

    themes->actions().at(1)->trigger();
    EXPECT_TRUE(m_theme.isDark());
    EXPECT_TRUE(themes->actions().at(1)->isChecked());
    EXPECT_FALSE(themes->actions().at(0)->isChecked());
    themes->actions().at(2)->trigger();
    EXPECT_EQ(m_theme.mode(), nexus::ui::ThemeMode::System);
    EXPECT_TRUE(themes->actions().at(2)->isChecked());
}

TEST_F(MainWindowTest, OpeningAVaultShowsLoadingThenTheWorkspace)
{
    makeVault(QStringLiteral("notes"), 3);
    MainWindow window(m_theme, m_settings);
    window.show();

    window.openVault(path(QStringLiteral("notes")));
    EXPECT_EQ(window.currentPage(), MainWindow::Page::Loading) << "visible while the worker runs";
    EXPECT_EQ(window.findChild<nexus::ui::VaultStatusWidget*>()->summary(),
              QStringLiteral("正在開啟 vault…"));

    ASSERT_TRUE(waitFor([&] { return window.currentPage() == MainWindow::Page::Workspace; }));
    EXPECT_EQ(window.windowTitle(), QStringLiteral("notes — Nexus"));
    EXPECT_TRUE(window.findChild<nexus::ui::VaultSidebar*>()->isVisible());
    EXPECT_EQ(window.findChild<nexus::ui::VaultSidebar*>()->vaultName(), QStringLiteral("notes"));
    EXPECT_EQ(window.findChild<nexus::ui::VaultStatusWidget*>()->summary(),
              QStringLiteral("監看中 3 篇筆記"));
}

TEST_F(MainWindowTest, OpenedVaultIsRememberedAndReopenedNextTime)
{
    makeVault(QStringLiteral("notes"), 1);
    {
        MainWindow window(m_theme, m_settings);
        window.show();
        window.openVault(path(QStringLiteral("notes")));
        ASSERT_TRUE(waitFor([&] { return window.currentPage() == MainWindow::Page::Workspace; }));
    }
    const nexus::ui::RecentVaults recent(m_settings);
    ASSERT_EQ(recent.entries().size(), 1);
    EXPECT_EQ(recent.lastOpenVault(),
              nexus::ui::RecentVaults::normalizedPath(path(QStringLiteral("notes"))));

    MainWindow again(m_theme, m_settings);
    again.show();
    again.restoreLastVault();
    ASSERT_TRUE(waitFor([&] { return again.currentPage() == MainWindow::Page::Workspace; }));
    EXPECT_EQ(again.session().name(), QStringLiteral("notes"));
}

TEST_F(MainWindowTest, MissingLastVaultFallsBackToWelcomeWithoutAnError)
{
    makeVault(QStringLiteral("notes"), 1);
    {
        MainWindow window(m_theme, m_settings);
        window.openVault(path(QStringLiteral("notes")));
        ASSERT_TRUE(waitFor([&] { return window.currentPage() == MainWindow::Page::Workspace; }));
    }
    QDir(path(QStringLiteral("notes"))).removeRecursively();

    MainWindow again(m_theme, m_settings);
    again.show();
    again.restoreLastVault();
    ASSERT_TRUE(
        waitFor([&] { return again.session().state() == nexus::ui::VaultSession::State::Closed; }));
    EXPECT_EQ(again.currentPage(), MainWindow::Page::Welcome);
    EXPECT_TRUE(again.findChildren<QMessageBox*>().isEmpty()) << "automatic reopen stays silent";
}

TEST_F(MainWindowTest, NothingToRestoreStaysOnWelcome)
{
    MainWindow window(m_theme, m_settings);
    window.restoreLastVault();
    EXPECT_EQ(window.session().state(), nexus::ui::VaultSession::State::Closed);
    EXPECT_EQ(window.currentPage(), MainWindow::Page::Welcome);
}

TEST_F(MainWindowTest, ClosingTheVaultReturnsToWelcomeAndForgetsAutoOpen)
{
    makeVault(QStringLiteral("notes"), 1);
    MainWindow window(m_theme, m_settings);
    window.show();
    window.openVault(path(QStringLiteral("notes")));
    ASSERT_TRUE(waitFor([&] { return window.currentPage() == MainWindow::Page::Workspace; }));

    const QMenu* file = findMenu(window, QStringLiteral("檔案"));
    QAction* close = findAction(file, QStringLiteral("關閉 vault"));
    ASSERT_TRUE(close->isEnabled());
    close->trigger();

    EXPECT_EQ(window.currentPage(), MainWindow::Page::Welcome);
    EXPECT_EQ(window.windowTitle(), QStringLiteral("Nexus"));
    EXPECT_FALSE(close->isEnabled());
    EXPECT_TRUE(nexus::ui::RecentVaults(m_settings).lastOpenVault().isEmpty());
    EXPECT_EQ(nexus::ui::RecentVaults(m_settings).entries().size(), 1)
        << "stays in the recent list";
}

TEST_F(MainWindowTest, SidebarToggleIsRememberedAndOnlyShowsWithAVault)
{
    makeVault(QStringLiteral("notes"), 1);
    MainWindow window(m_theme, m_settings);
    window.show();
    auto* sidebar = window.findChild<nexus::ui::VaultSidebar*>();
    QAction* toggle =
        findAction(findMenu(window, QStringLiteral("檢視")), QStringLiteral("顯示檔案樹"));

    EXPECT_TRUE(toggle->isChecked());
    EXPECT_FALSE(sidebar->isVisible()) << "no vault yet";

    window.openVault(path(QStringLiteral("notes")));
    ASSERT_TRUE(waitFor([&] { return window.currentPage() == MainWindow::Page::Workspace; }));
    EXPECT_TRUE(sidebar->isVisible());

    toggle->trigger();
    EXPECT_FALSE(sidebar->isVisible());
    EXPECT_FALSE(m_settings.value(QStringLiteral("view/sidebar")).toBool());
    toggle->trigger();
    EXPECT_TRUE(sidebar->isVisible());
}

TEST_F(MainWindowTest, WindowGeometryIsRemembered)
{
    // The offscreen screen is only 800x600, restoreGeometry never exceeds the screen.
    {
        MainWindow window(m_theme, m_settings);
        window.resize(800, 560);
        window.show();
        window.close();
    }
    MainWindow again(m_theme, m_settings);
    again.show();
    EXPECT_EQ(again.size(), QSize(800, 560));
}

TEST_F(MainWindowTest, RecentMenuListsVaultsAndCanBeCleared)
{
    makeVault(QStringLiteral("notes"), 1);
    MainWindow window(m_theme, m_settings);
    window.openVault(path(QStringLiteral("notes")));
    ASSERT_TRUE(waitFor([&] { return window.currentPage() == MainWindow::Page::Workspace; }));

    QMenu* recent =
        findAction(findMenu(window, QStringLiteral("檔案")), QStringLiteral("最近開啟"))->menu();
    emit recent->aboutToShow();
    ASSERT_GE(recent->actions().size(), 2);
    EXPECT_TRUE(recent->actions().first()->text().startsWith(QStringLiteral("notes")));
    EXPECT_EQ(recent->actions().last()->text(), QStringLiteral("清除清單"));

    recent->actions().last()->trigger();
    EXPECT_TRUE(nexus::ui::RecentVaults(m_settings).entries().isEmpty());
    emit recent->aboutToShow();
    ASSERT_EQ(recent->actions().size(), 1);
    EXPECT_FALSE(recent->actions().first()->isEnabled());
}

TEST_F(MainWindowTest, ActionsThatNeedTheEditorAreDisabledForNow)
{
    MainWindow window(m_theme, m_settings);
    const QMenu* file = findMenu(window, QStringLiteral("檔案"));
    for (const char* text : {"新增筆記", "儲存", "關閉分頁"}) {
        EXPECT_FALSE(findAction(file, QString::fromUtf8(text))->isEnabled()) << text;
    }
}
