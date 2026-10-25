#include "ui/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>

#include "ui/Icons.h"
#include "ui/ThemeManager.h"

namespace nexus::ui {

MainWindow::MainWindow(ThemeManager& theme, QWidget* parent) : QMainWindow(parent), m_theme(theme)
{
    setWindowTitle(QStringLiteral("Nexus"));
    resize(1280, 800);

    buildMenus();
    statusBar();

    connect(&m_theme, &ThemeManager::themeChanged, this, &MainWindow::refreshThemeChecks);
    refreshThemeChecks();
}

void MainWindow::buildMenus()
{
    // Menus are children of the menu bar, which is owned by this window.
    QMenu* fileMenu = menuBar()->addMenu(tr("檔案(&F)"));
    fileMenu->addAction(tr("結束(&Q)"), QKeySequence::Quit, this, &QWidget::close);
    menuBar()->addMenu(tr("編輯(&E)"));

    QMenu* viewMenu = menuBar()->addMenu(tr("檢視(&V)"));
    QMenu* themeMenu = viewMenu->addMenu(tr("主題(&T)"));
    m_themeGroup = new QActionGroup(this);
    const auto addMode = [this, themeMenu](const QString& text, ThemeMode mode) {
        QAction* action = themeMenu->addAction(text);
        action->setCheckable(true);
        m_themeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] { m_theme.setMode(mode); });
        return action;
    };
    m_lightAction = addMode(tr("淺色・森林綠"), ThemeMode::Light);
    m_darkAction = addMode(tr("深色・森林綠"), ThemeMode::Dark);
    m_systemAction = addMode(tr("跟隨系統"), ThemeMode::System);

    menuBar()->addMenu(tr("說明(&H)"));
}

void MainWindow::refreshThemeChecks()
{
    // The QSS hides the platform check indicator, so the checked entry gets a tinted icon.
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
}

} // namespace nexus::ui
