#include "ui/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

#include "ui/EditorStatusWidget.h"
#include "ui/EditorTabs.h"
#include "ui/Icons.h"
#include "ui/LoadingPage.h"
#include "ui/NoteEditor.h"
#include "ui/NoteSession.h"
#include "ui/ThemeManager.h"
#include "ui/UiText.h"
#include "ui/VaultSidebar.h"
#include "ui/VaultStatusWidget.h"
#include "ui/WelcomePage.h"

#ifndef NEXUS_VERSION
#define NEXUS_VERSION "0.0.0"
#endif

namespace nexus::ui {

namespace {

constexpr auto kGeometryKey = "window/geometry";
constexpr auto kSplitterKey = "window/splitter";
constexpr auto kSidebarKey = "view/sidebar";
constexpr auto kLineNumbersKey = "view/lineNumbers";
constexpr int kSidebarWidth = 248;
constexpr int kSidebarMinWidth = 200;
constexpr int kSidebarMaxWidth = 280;

} // namespace

MainWindow::MainWindow(ThemeManager& theme, QSettings& settings, QWidget* parent)
    : QMainWindow(parent), m_theme(theme), m_settings(settings), m_recent(settings)
{
    setWindowTitle(QStringLiteral("Nexus"));
    setMinimumSize(800, 520);

    buildMenus();
    buildContent();
    m_sidebar->setSession(m_session);
    connect(m_newNoteAction, &QAction::triggered, m_sidebar, &VaultSidebar::newNote);

    connect(&m_theme, &ThemeManager::themeChanged, this, &MainWindow::refreshThemeChecks);
    connect(&m_session, &VaultSession::opening, this, &MainWindow::onOpening);
    connect(&m_session, &VaultSession::opened, this, &MainWindow::onOpened);
    connect(&m_session, &VaultSession::openFailed, this, &MainWindow::onOpenFailed);
    connect(&m_session, &VaultSession::closed, this, &MainWindow::onClosed);
    connect(&m_session, &VaultSession::noteCountChanged, this, [this](int count) {
        m_sidebar->setNoteCount(count);
        m_status->setState(m_session.state(), count);
    });

    refreshThemeChecks();
    refreshActions();
    m_welcome->setRecent(m_recent.entries());

    if (!restoreGeometry(m_settings.value(QLatin1StringView(kGeometryKey)).toByteArray())) {
        resize(1280, 800);
    }
}

MainWindow::~MainWindow() = default;

MainWindow::Page MainWindow::currentPage() const
{
    return static_cast<Page>(m_stack->currentIndex());
}

// ---- construction -------------------------------------------------------------------------

void MainWindow::buildMenus()
{
    // Menus are children of the menu bar, which is owned by this window.
    QMenu* fileMenu = menuBar()->addMenu(tr("檔案"));
    m_openAction = fileMenu->addAction(tr("開啟 vault…"), QKeySequence::Open, this,
                                       &MainWindow::chooseAndOpenVault);
    m_createAction =
        fileMenu->addAction(tr("建立新 vault…"), this, &MainWindow::chooseAndCreateVault);
    m_recentMenu = fileMenu->addMenu(tr("最近開啟"));
    connect(m_recentMenu, &QMenu::aboutToShow, this, &MainWindow::refreshRecentMenu);
    fileMenu->addSeparator();
    m_newNoteAction = fileMenu->addAction(tr("新增筆記"));
    m_newNoteAction->setShortcut(QKeySequence::New);
    m_saveAction = fileMenu->addAction(tr("儲存"), QKeySequence::Save, this,
                                       [this] { m_tabs->saveCurrent(); });
    m_closeTabAction = fileMenu->addAction(tr("關閉分頁"), QKeySequence::Close, this,
                                           [this] { m_tabs->closeCurrentTab(); });
    fileMenu->addSeparator();
    m_closeVaultAction = fileMenu->addAction(tr("關閉 vault"), this, [this] { m_session.close(); });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("結束"), QKeySequence::Quit, this, &QWidget::close);

    // Forwarded to the editor of the current tab. The editor handles the same keys itself while
    // it has focus, so the shortcuts are only shown here and never steal keys from it.
    QMenu* editMenu = menuBar()->addMenu(tr("編輯"));
    const auto addEdit = [this, editMenu](const QString& text, QKeySequence::StandardKey key,
                                          void (QPlainTextEdit::*slot)()) {
        QAction* action = editMenu->addAction(text, this, [this, slot] {
            if (NoteEditor* editor = m_tabs->currentEditor()) {
                (editor->*slot)();
            }
        });
        action->setShortcut(key);
        action->setShortcutContext(Qt::WidgetShortcut);
        m_editActions.append(action);
    };
    addEdit(tr("復原"), QKeySequence::Undo, &QPlainTextEdit::undo);
    addEdit(tr("重做"), QKeySequence::Redo, &QPlainTextEdit::redo);
    editMenu->addSeparator();
    addEdit(tr("剪下"), QKeySequence::Cut, &QPlainTextEdit::cut);
    addEdit(tr("複製"), QKeySequence::Copy, &QPlainTextEdit::copy);
    addEdit(tr("貼上"), QKeySequence::Paste, &QPlainTextEdit::paste);
    editMenu->addSeparator();
    addEdit(tr("全選"), QKeySequence::SelectAll, &QPlainTextEdit::selectAll);

    QMenu* viewMenu = menuBar()->addMenu(tr("檢視"));
    m_themeMenu = viewMenu->addMenu(tr("主題"));
    m_themeGroup = new QActionGroup(this);
    const auto addMode = [this](const QString& text, ThemeMode mode) {
        QAction* action = m_themeMenu->addAction(text);
        action->setCheckable(true);
        m_themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] { m_theme.setMode(mode); });
        return action;
    };
    m_lightAction = addMode(tr("淺色・森林綠"), ThemeMode::Light);
    m_darkAction = addMode(tr("深色・森林綠"), ThemeMode::Dark);
    m_systemAction = addMode(tr("跟隨系統"), ThemeMode::System);
    viewMenu->addSeparator();

    m_sidebarAction = viewMenu->addAction(tr("顯示檔案樹"));
    m_sidebarAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
    m_sidebarAction->setCheckable(true);
    m_sidebarAction->setChecked(m_settings.value(QLatin1StringView(kSidebarKey), true).toBool());
    connect(m_sidebarAction, &QAction::toggled, this, [this](bool shown) {
        m_settings.setValue(QLatin1StringView(kSidebarKey), shown);
        m_sidebar->setVisible(shown && m_session.isOpen());
        refreshThemeChecks();
    });

    m_lineNumbersAction = viewMenu->addAction(tr("顯示行號"));
    m_lineNumbersAction->setCheckable(true);
    m_lineNumbersAction->setChecked(
        m_settings.value(QLatin1StringView(kLineNumbersKey), true).toBool());
    connect(m_lineNumbersAction, &QAction::toggled, this, [this](bool shown) {
        m_settings.setValue(QLatin1StringView(kLineNumbersKey), shown);
        m_tabs->setLineNumbersVisible(shown);
        refreshThemeChecks();
    });

    QMenu* helpMenu = menuBar()->addMenu(tr("說明"));
    helpMenu->addAction(tr("關於 Nexus"), this, &MainWindow::showAbout);
}

void MainWindow::buildContent()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(1);

    m_sidebar = new VaultSidebar(m_theme, &m_settings, m_splitter);
    m_sidebar->setMinimumWidth(kSidebarMinWidth);
    m_sidebar->setMaximumWidth(kSidebarMaxWidth);
    m_sidebar->hide(); // only while a vault is open

    m_stack = new QStackedWidget(m_splitter);
    m_welcome = new WelcomePage(m_theme, m_stack);
    m_loading = new LoadingPage(m_stack);
    auto* workspace = new QFrame(m_stack);
    workspace->setObjectName(QStringLiteral("workspace"));
    auto* workspaceLayout = new QVBoxLayout(workspace);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    m_tabs = new EditorTabs(m_theme, workspace);
    m_tabs->setLineNumbersVisible(m_lineNumbersAction->isChecked());
    workspaceLayout->addWidget(m_tabs);
    connect(m_tabs, &EditorTabs::currentChanged, this, [this] {
        refreshTitle();
        refreshActions();
    });
    // File tree <-> editor.
    connect(m_sidebar, &VaultSidebar::openNoteRequested, m_tabs, &EditorTabs::openNote);
    connect(m_sidebar, &VaultSidebar::noteRenamed, m_tabs, &EditorTabs::renameNote);
    connect(m_sidebar, &VaultSidebar::noteRemoved, m_tabs, &EditorTabs::removeNote);
    connect(m_tabs, &EditorTabs::dirtyChanged, m_sidebar, &VaultSidebar::setNoteDirty);
    connect(m_tabs, &EditorTabs::currentChanged, this, [this] {
        if (const NoteSession* note = m_tabs->currentSession()) {
            m_sidebar->selectNote(note->relPath());
        }
    });
    connect(m_tabs, &EditorTabs::saveFailed, this,
            [this](const QString& relPath, const QString& message) {
                statusBar()->showMessage(QStringLiteral("%1：%2").arg(relPath, message), 8000);
            });
    m_stack->addWidget(m_welcome); // Page::Welcome
    m_stack->addWidget(m_loading); // Page::Loading
    m_stack->addWidget(workspace); // Page::Workspace

    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_stack);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({kSidebarWidth, 1000});
    m_splitter->restoreState(m_settings.value(QLatin1StringView(kSplitterKey)).toByteArray());
    setCentralWidget(m_splitter);

    m_status = new VaultStatusWidget(m_theme, this);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_status);
    m_editorStatus = new EditorStatusWidget(*m_tabs, this);
    statusBar()->addPermanentWidget(m_editorStatus);

    connect(m_welcome, &WelcomePage::openFolderRequested, this, &MainWindow::chooseAndOpenVault);
    connect(m_welcome, &WelcomePage::createVaultRequested, this, &MainWindow::chooseAndCreateVault);
    connect(m_welcome, &WelcomePage::recentActivated, this,
            [this](const QString& path) { openVault(path); });
    connect(m_welcome, &WelcomePage::removeRecentRequested, this, [this](const QString& path) {
        m_recent.remove(path);
        m_welcome->setRecent(m_recent.entries());
    });
    connect(m_welcome, &WelcomePage::clearRecentRequested, this, [this] {
        m_recent.clear();
        m_welcome->setRecent(m_recent.entries());
    });
    connect(m_welcome, &WelcomePage::quickStartRequested, this, &MainWindow::showQuickStart);
    connect(m_welcome, &WelcomePage::appearanceRequested, this,
            [this] { m_themeMenu->popup(QCursor::pos()); });
}

// ---- state --------------------------------------------------------------------------------

void MainWindow::refreshThemeChecks()
{
    // The QSS hides the platform check indicator, so a checked entry gets a tinted icon.
    const QIcon checkIcon = icons::icon(QStringLiteral("check"), m_theme.palette().accent, 14);
    struct Entry {
        QAction* action;
        ThemeMode mode;
    };
    for (const Entry entry :
         {Entry{m_lightAction, ThemeMode::Light}, Entry{m_darkAction, ThemeMode::Dark},
          Entry{m_systemAction, ThemeMode::System}}) {
        const bool current = entry.mode == m_theme.mode();
        entry.action->setChecked(current);
        entry.action->setIcon(current ? checkIcon : QIcon());
    }
    for (QAction* action : {m_sidebarAction, m_lineNumbersAction}) {
        if (action != nullptr) {
            action->setIcon(action->isChecked() ? checkIcon : QIcon());
        }
    }
}

void MainWindow::refreshActions()
{
    m_closeVaultAction->setEnabled(m_session.state() != VaultSession::State::Closed);
    m_newNoteAction->setEnabled(m_session.isOpen());
    const bool idle = m_session.state() != VaultSession::State::Opening;
    m_openAction->setEnabled(idle);
    m_createAction->setEnabled(idle);
    m_recentMenu->setEnabled(idle);
    const bool hasTab = m_tabs->count() > 0;
    m_saveAction->setEnabled(hasTab);
    m_closeTabAction->setEnabled(hasTab);
    for (QAction* action : m_editActions) {
        action->setEnabled(hasTab);
    }
}

void MainWindow::refreshTitle()
{
    const NoteSession* note = m_tabs->currentSession();
    setWindowTitle(
        formatWindowTitle(note != nullptr ? QFileInfo(note->relPath()).fileName() : QString(),
                          m_session.isOpen() ? m_session.name() : QString()));
}

void MainWindow::refreshRecentMenu()
{
    m_recentMenu->clear();
    const QList<RecentVault> entries = m_recent.entries();
    if (entries.isEmpty()) {
        m_recentMenu->addAction(tr("（沒有最近開啟的 vault）"))->setEnabled(false);
        return;
    }
    for (const RecentVault& entry : entries) {
        m_recentMenu->addAction(RecentVaults::displayName(entry.path) + QStringLiteral("    ") +
                                    QDir::toNativeSeparators(entry.path),
                                this, [this, path = entry.path] { openVault(path); });
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction(tr("清除清單"), this, [this] {
        m_recent.clear();
        m_welcome->setRecent(m_recent.entries());
    });
}

// ---- opening ------------------------------------------------------------------------------

void MainWindow::openVault(const QString& path, bool silent)
{
    m_silentOpen = silent;
    m_session.open(path);
}

void MainWindow::restoreLastVault()
{
    const QString last = m_recent.lastOpenVault();
    if (!last.isEmpty()) {
        openVault(last, /*silent=*/true);
    }
}

void MainWindow::chooseAndOpenVault()
{
    const QString start = m_session.isOpen() ? m_session.root() : QString();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("選擇 vault 資料夾"), start);
    if (!dir.isEmpty()) {
        openVault(dir);
    }
}

void MainWindow::chooseAndCreateVault()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("選擇一個空資料夾來建立 vault"));
    if (!dir.isEmpty()) {
        m_silentOpen = false;
        m_session.createAndOpen(dir);
    }
}

void MainWindow::onOpening(const QString& path)
{
    // The tabs keep the old vault's NoteFiles alive, so they can still be written.
    m_tabs->saveAllAndWait();
    m_tabs->setVault(nullptr, QString());
    m_loading->setVaultName(RecentVaults::displayName(path));
    m_welcome->setBusy(true);
    m_sidebar->hide();
    m_stack->setCurrentIndex(static_cast<int>(Page::Loading));
    m_status->setState(VaultSession::State::Opening, 0);
    refreshTitle();
    refreshActions();
}

void MainWindow::onOpened()
{
    m_recent.add(m_session.root());
    m_recent.setLastOpenVault(m_session.root());
    m_welcome->setBusy(false);
    m_welcome->setRecent(m_recent.entries());

    m_sidebar->setVault(m_session.name(), m_session.noteCount());
    m_tabs->setVault(m_session.files(), m_session.name());
    m_sidebar->setVisible(m_sidebarAction->isChecked());
    m_stack->setCurrentIndex(static_cast<int>(Page::Workspace));
    m_status->setState(VaultSession::State::Open, m_session.noteCount());
    refreshTitle();
    refreshActions();
    m_silentOpen = false;
}

void MainWindow::onOpenFailed(const QString& /*path*/, const VaultOpenError& error)
{
    m_welcome->setBusy(false);
    m_welcome->setRecent(m_recent.entries());
    m_sidebar->hide();
    m_stack->setCurrentIndex(static_cast<int>(Page::Welcome));
    m_status->setState(VaultSession::State::Closed, 0);
    refreshTitle();
    refreshActions();
    if (!m_silentOpen) {
        showError(describeOpenError(error));
    }
    m_silentOpen = false;
}

void MainWindow::onClosed()
{
    m_tabs->saveAllAndWait();
    m_tabs->setVault(nullptr, QString());
    // Closing on purpose: do not reopen this vault at the next start.
    m_recent.setLastOpenVault(QString());
    m_welcome->setBusy(false);
    m_welcome->setRecent(m_recent.entries());
    m_sidebar->hide();
    m_stack->setCurrentIndex(static_cast<int>(Page::Welcome));
    m_status->setState(VaultSession::State::Closed, 0);
    refreshTitle();
    refreshActions();
}

// ---- dialogs ------------------------------------------------------------------------------

void MainWindow::showError(const QString& text)
{
    // open() instead of exec(): shows the box without a nested event loop.
    auto* box =
        new QMessageBox(QMessageBox::Warning, tr("無法開啟 vault"), text, QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

void MainWindow::showQuickStart()
{
    auto* box = new QMessageBox(QMessageBox::Information, tr("快速入門"),
                                tr("1. 開啟一個放 Markdown 筆記的資料夾，或建立新的 vault。\n"
                                   "2. 在筆記中用 [[雙括號]] 連結其他筆記，用 #標籤 分類。\n"
                                   "3. 編輯會在停止輸入後自動儲存。"),
                                QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

void MainWindow::showAbout()
{
    auto* box = new QMessageBox(
        QMessageBox::Information, tr("關於 Nexus"),
        tr("Nexus %1\n知識圖驅動的 AI 開發環境。").arg(QStringLiteral(NEXUS_VERSION)),
        QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_tabs->saveAllAndWait();
    m_settings.setValue(QLatin1StringView(kGeometryKey), saveGeometry());
    m_settings.setValue(QLatin1StringView(kSplitterKey), m_splitter->saveState());
    QMainWindow::closeEvent(event);
}

} // namespace nexus::ui
