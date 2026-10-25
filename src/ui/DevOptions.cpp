#include "ui/DevOptions.h"

#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QSettings>
#include <QTimer>
#include <QWidget>

#include "ui/EditorTabs.h"
#include "ui/MainWindow.h"
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

bool applyDeveloperOptions(const QStringList& args, ThemeManager& theme, MainWindow& window)
{
    const QString themeName = valueOf(args, QStringLiteral("--theme"));
    if (!themeName.isEmpty()) {
        theme.setMode(ThemeManager::fromString(themeName), /*persist=*/false);
    }

    const QStringList size = valueOf(args, QStringLiteral("--size")).split(QLatin1Char('x'));
    if (size.size() == 2) {
        window.resize(size.at(0).toInt(), size.at(1).toInt());
    }

    const QString vault = valueOf(args, QStringLiteral("--vault"));
    const bool opened = !vault.isEmpty();
    if (opened) {
        window.openVault(vault);
        // --open a.md,b.md: open these notes as tabs once the vault is open (last one current).
        const QStringList notes =
            valueOf(args, QStringLiteral("--open")).split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (!notes.isEmpty()) {
            auto* once = new QObject(&window); // deleted after the first opened()
            QObject::connect(&window.session(), &VaultSession::opened, once,
                             [&window, notes, once] {
                                 for (const QString& note : notes) {
                                     window.tabs().openNote(note, /*newTab=*/true);
                                 }
                                 once->deleteLater();
                             });
        }
    }

    const QString menuPath = valueOf(args, QStringLiteral("--menu"));
    const QString menuShot = valueOf(args, QStringLiteral("--menu-screenshot"));
    if (!menuPath.isEmpty() && !menuShot.isEmpty()) {
        QTimer::singleShot(800, &window, [&window, menuPath, menuShot] {
            QMenu* menu = findMenu(window, menuPath);
            if (menu == nullptr) {
                QApplication::exit(2);
                return;
            }
            menu->popup(window.mapToGlobal(QPoint(40, 60)));
            QTimer::singleShot(500, menu, [menu, menuShot] {
                QApplication::exit(menu->grab().save(menuShot) ? 0 : 1);
            });
        });
        return opened;
    }

    const QString shot = valueOf(args, QStringLiteral("--screenshot"));
    if (!shot.isEmpty()) {
        // Give the window time to lay out and any background work to finish before grabbing.
        QTimer::singleShot(1500, &window, [&window, shot] {
            const bool saved = window.grab().save(shot);
            QApplication::exit(saved ? 0 : 1);
        });
    }
    return opened;
}

std::unique_ptr<QSettings> makeSettings(const QStringList& args)
{
    const QString file = valueOf(args, QStringLiteral("--settings"));
    if (file.isEmpty()) {
        return std::make_unique<QSettings>();
    }
    return std::make_unique<QSettings>(file, QSettings::IniFormat);
}

} // namespace nexus::ui
