#pragma once

#include <memory>

#include <QStringList>

class QSettings;

namespace nexus::ui {

class MainWindow;
class ThemeManager;

// Developer / documentation command-line options (not user facing):
//   --theme light|dark|system   override the theme for this run only (not saved)
//   --vault <folder>            open this vault now
//   --open <a.md,b.md>          with --vault: open these notes in tabs
//   --settings <file.ini>       use a separate settings file (see makeSettings)
//   --size WxH                  resize the main window
//   --screenshot <file.png>     save a picture of the window and quit
//   --menu A/B --menu-screenshot <file.png>   open menu A > B, save a picture of it, quit
// Unknown arguments are ignored so other tools may add their own. Call after window.show().
// Returns true when --vault opened a vault (the caller then skips re-opening the last one).
bool applyDeveloperOptions(const QStringList& args, ThemeManager& theme, MainWindow& window);

// Settings store: --settings <file.ini> selects a separate ini file (for reproducible
// screenshots); otherwise the normal per-user settings.
std::unique_ptr<QSettings> makeSettings(const QStringList& args);

} // namespace nexus::ui
