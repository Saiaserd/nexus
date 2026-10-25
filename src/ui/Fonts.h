#pragma once

#include <QFont>
#include <QString>

namespace nexus::ui::fonts {

// Registers the embedded JetBrains Mono and sets the application font (ADR 0006: system UI
// fonts, only the monospace face is embedded). Safe to call more than once.
void install();

// Family name of the embedded monospace font ("JetBrains Mono"), or a system fallback when the
// embedded file could not be loaded.
QString monoFamily();

// Monospace font with the given pixel size (the design is specified in px).
QFont monoFont(int pixelSize, QFont::Weight weight = QFont::Normal);

} // namespace nexus::ui::fonts
