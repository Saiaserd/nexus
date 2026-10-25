#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace nexus::ui::icons {

// Line icons live in the resource file as SVG using `currentColor` (ADR 0006); they are tinted
// at run time with a palette colour. `name` is the file name without extension, e.g. "folder".
QPixmap pixmap(const QString& name, const QColor& color, int logicalSize, qreal devicePixelRatio);

// Icon with 1x and 2x pixmaps so it stays sharp on high-DPI screens.
QIcon icon(const QString& name, const QColor& color, int logicalSize);

} // namespace nexus::ui::icons
