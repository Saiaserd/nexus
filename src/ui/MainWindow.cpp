#include "ui/MainWindow.h"

#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>

namespace nexus::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Nexus"));
    resize(1280, 800);

    // Menus are children of the menu bar, which is owned by this window.
    QMenu* fileMenu = menuBar()->addMenu(tr("檔案(&F)"));
    fileMenu->addAction(tr("結束(&Q)"), QKeySequence::Quit, this, &QWidget::close);
    menuBar()->addMenu(tr("編輯(&E)"));
    menuBar()->addMenu(tr("檢視(&V)"));
    menuBar()->addMenu(tr("說明(&H)"));

    statusBar();
}

} // namespace nexus::ui
