#pragma once

#include <cstdint>

#include <QList>
#include <QMainWindow>

#include "ui/RecentVaults.h"
#include "ui/VaultSession.h"

class QAction;
class QActionGroup;
class QMenu;
class QSettings;
class QSplitter;
class QStackedWidget;

namespace nexus::ui {

class EditorStatusWidget;
class EditorTabs;
class LoadingPage;
class ThemeManager;
class VaultSidebar;
class VaultStatusWidget;
class WelcomePage;

// Application shell: menus, the sidebar and the work area (welcome page, loading page or the
// workspace), status bar. `settings` and `theme` must outlive the window.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    enum class Page : std::uint8_t { Welcome, Loading, Workspace };

    MainWindow(ThemeManager& theme, QSettings& settings, QWidget* parent = nullptr);
    ~MainWindow() override;

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(MainWindow&&) = delete;

    [[nodiscard]] VaultSession& session() { return m_session; }
    [[nodiscard]] Page currentPage() const;
    [[nodiscard]] EditorTabs& tabs() const { return *m_tabs; }

    // Opens a vault in the background. `silent` = a failure only returns to the welcome page
    // (used for the automatic re-open at start-up) instead of showing an error message.
    void openVault(const QString& path, bool silent = false);
    // Re-opens the vault that was open when the application last ran, if any.
    void restoreLastVault();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildMenus();
    void buildContent();
    void refreshThemeChecks();
    void refreshRecentMenu();
    void refreshActions();
    void refreshTitle();

    void chooseAndOpenVault();
    void chooseAndCreateVault();
    void showError(const QString& text);
    void showQuickStart();
    void showAbout();

    void onOpening(const QString& path);
    void onOpened();
    void onOpenFailed(const QString& path, const VaultOpenError& error);
    void onClosed();

    ThemeManager& m_theme;
    QSettings& m_settings;
    RecentVaults m_recent;
    VaultSession m_session;
    bool m_silentOpen = false;

    QSplitter* m_splitter = nullptr;
    VaultSidebar* m_sidebar = nullptr;
    QStackedWidget* m_stack = nullptr;
    WelcomePage* m_welcome = nullptr;
    LoadingPage* m_loading = nullptr;
    VaultStatusWidget* m_status = nullptr;
    EditorTabs* m_tabs = nullptr;
    EditorStatusWidget* m_editorStatus = nullptr;

    QMenu* m_recentMenu = nullptr;
    QMenu* m_themeMenu = nullptr;
    QActionGroup* m_themeGroup = nullptr;
    QAction* m_lightAction = nullptr;
    QAction* m_darkAction = nullptr;
    QAction* m_systemAction = nullptr;

    QAction* m_openAction = nullptr;
    QAction* m_createAction = nullptr;
    QAction* m_newNoteAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_closeTabAction = nullptr;
    QAction* m_closeVaultAction = nullptr;
    QAction* m_sidebarAction = nullptr;
    QAction* m_lineNumbersAction = nullptr;
    QList<QAction*> m_editActions;
};

} // namespace nexus::ui
