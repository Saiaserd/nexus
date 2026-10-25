#pragma once

#include <QMainWindow>

namespace nexus::ui {

// Empty application shell (T-006): title, default size, empty menus and a status bar.
// Native look only; no theming or custom styling until the UI design is approved.
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
};

} // namespace nexus::ui
