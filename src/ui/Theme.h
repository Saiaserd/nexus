#pragma once

#include <QColor>
#include <QList>
#include <QPair>
#include <QPalette>
#include <QString>

namespace nexus::ui {

// Every colour used by the UI. The first group is copied verbatim from the approved design
// (docs/design/m1/Main.dc.html, renderVals()); the second group is taken from the literal
// colours in Welcome.dc.html / States.dc.html. The dark values of the second group are not in
// the design and were chosen by the UI developer to match the dark main palette (see TASKS.md).
// Nothing outside Theme.cpp may hard-code a colour.
struct Palette {
    // --- Main.dc.html -----------------------------------------------------------------
    QColor bg;
    QColor chrome;
    QColor panel;
    QColor surface;
    QColor border;
    QColor borderSoft;
    QColor text;
    QColor muted;
    QColor accent;
    QColor accentSoft;
    QColor accentText;
    QColor marker;
    QColor gutter;
    QColor gutterActive;
    QColor lineActive;
    QColor link;
    QColor tag;
    QColor tagBg;
    QColor codeBg;
    QColor kw;
    QColor ok;

    // --- Welcome.dc.html / States.dc.html ---------------------------------------------
    QColor card;           // cards, menus, inputs, secondary buttons (#FFFFFF)
    QColor onAccent;       // text on an accent-filled button
    QColor onAccentMuted;  // secondary text on an accent-filled button
    QColor accentHover;    // link / primary button hover
    QColor hover;          // hovered / keyboard-focused row (#E2EBDE)
    QColor bodyText;       // long dialog text (#3C4A3F)
    QColor scrim;          // dim area behind a modal dialog (#C9D1C5)
    QColor danger;         // error text, destructive button (#9A3412)
    QColor dangerLine;     // error icon / input border (#B03A0B)
    QColor dangerBg;       // error message background (#FBE9E1)
    QColor warnBg;         // conflict bar background (#FDF3DC)
    QColor warnLine;       // conflict bar border (#EBD9A8)
    QColor warnButtonLine; // button border inside the conflict bar (#C9B07A)
    QColor warnText;       // conflict bar text (#5C4105)
    QColor warnIcon;       // conflict bar icon (#7A5607)

    // --- Not in the design: scrollbars (the design is a static mock-up) ----------------
    QColor scroll;
    QColor scrollHover;

    bool dark = false;
};

const Palette& lightPalette();
const Palette& darkPalette();

// Name / colour pairs for every palette member, in declaration order. The names are the
// `@token@` names used by the style sheet template and by the tests.
QList<QPair<QString, QColor>> paletteEntries(const Palette& palette);

// WCAG 2.x contrast ratio between two opaque colours, 1.0 .. 21.0.
double contrastRatio(const QColor& a, const QColor& b);

// Qt palette for the Fusion style (window, base, text, highlight, disabled group ...).
QPalette buildQPalette(const Palette& palette);

// Expands the QSS template (resources/app.qss.in): every `@name@` becomes the palette colour.
QString buildStyleSheet(const QString& templateText, const Palette& palette);

} // namespace nexus::ui
