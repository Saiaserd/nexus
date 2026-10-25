#pragma once

#include <QStringList>

class QWidget;

namespace nexus::ui {

class ThemeManager;

// Developer / documentation command-line options (not user facing):
//   --theme light|dark|system   override the theme for this run only (not saved)
//   --size WxH                  resize the main window
//   --screenshot <file.png>     save a picture of the window and quit
//   --menu A/B --menu-screenshot <file.png>   open menu A > B, save a picture of it, quit
// Unknown arguments are ignored so other tools may add their own. Call after window.show().
void applyDeveloperOptions(const QStringList& args, ThemeManager& theme, QWidget& window);

} // namespace nexus::ui
