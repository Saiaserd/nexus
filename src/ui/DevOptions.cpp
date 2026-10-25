#include "ui/DevOptions.h"

#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include "ui/ThemeManager.h"

namespace nexus::ui {

namespace {

QString valueOf(const QStringList& args, const QString& name)
{
    const qsizetype index = args.indexOf(name);
    if (index < 0 || index + 1 >= args.size()) {
        return {};
    }
    return args.at(index + 1);
}

// Finds a menu by a path such as "檢視/主題"; titles are compared without "&" and "(X)" marks.
QMenu* findMenu(QMainWindow& window, const QString& path)
{
    const auto plain = [](QString title) {
        title.remove(QLatin1Char('&'));
        const qsizetype paren = title.indexOf(QLatin1Char('('));
        return paren > 0 ? title.left(paren) : title;
    };
    QMenu* menu = nullptr;
    QList<QAction*> actions = window.menuBar()->actions();
    for (const QString& part : path.split(QLatin1Char('/'))) {
        menu = nullptr;
        for (QAction* action : actions) {
            if (action->menu() != nullptr && plain(action->text()) == part) {
                menu = action->menu();
                break;
            }
        }
        if (menu == nullptr) {
            return nullptr;
        }
        actions = menu->actions();
    }
    return menu;
}

} // namespace

void applyDeveloperOptions(const QStringList& args, ThemeManager& theme, QWidget& window)
{
    const QString themeName = valueOf(args, QStringLiteral("--theme"));
    if (!themeName.isEmpty()) {
        theme.setMode(ThemeManager::fromString(themeName), /*persist=*/false);
    }

    const QStringList size = valueOf(args, QStringLiteral("--size")).split(QLatin1Char('x'));
    if (size.size() == 2) {
        window.resize(size.at(0).toInt(), size.at(1).toInt());
    }

    const QString menuPath = valueOf(args, QStringLiteral("--menu"));
    const QString menuShot = valueOf(args, QStringLiteral("--menu-screenshot"));
    if (!menuPath.isEmpty() && !menuShot.isEmpty()) {
        if (auto* mainWindow = qobject_cast<QMainWindow*>(&window)) {
            QTimer::singleShot(500, &window, [mainWindow, menuPath, menuShot] {
                QMenu* menu = findMenu(*mainWindow, menuPath);
                if (menu == nullptr) {
                    QApplication::exit(2);
                    return;
                }
                menu->popup(mainWindow->mapToGlobal(QPoint(40, 60)));
                QTimer::singleShot(500, menu, [menu, menuShot] {
                    QApplication::exit(menu->grab().save(menuShot) ? 0 : 1);
                });
            });
        }
        return;
    }

    const QString shot = valueOf(args, QStringLiteral("--screenshot"));
    if (!shot.isEmpty()) {
        // Give the window time to lay out and any background work to finish before grabbing.
        QTimer::singleShot(1500, &window, [&window, shot] {
            const bool saved = window.grab().save(shot);
            QApplication::exit(saved ? 0 : 1);
        });
    }
}

} // namespace nexus::ui
