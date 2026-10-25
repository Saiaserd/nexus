#pragma once

#include <QMainWindow>

class QAction;
class QActionGroup;

namespace nexus::ui {

class ThemeManager;

// Application shell. T-110: menus (including View > Theme), themed menu bar and status bar.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(ThemeManager& theme, QWidget* parent = nullptr);

private:
    void buildMenus();
    void refreshThemeChecks();

    ThemeManager& m_theme;
    QActionGroup* m_themeGroup = nullptr;
    QAction* m_lightAction = nullptr;
    QAction* m_darkAction = nullptr;
    QAction* m_systemAction = nullptr;
};

} // namespace nexus::ui
