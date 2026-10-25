#include "ui/Theme.h"

#include <algorithm>
#include <cmath>

namespace nexus::ui {

namespace {

QColor hex(const char* value)
{
    return QColor::fromString(QLatin1StringView(value));
}

Palette makeLight()
{
    Palette p;
    p.bg = hex("#F6F8F4");
    p.chrome = hex("#E8EEE4");
    p.panel = hex("#EDF2EA");
    p.surface = hex("#FBFCFA");
    p.border = hex("#D3DDCE");
    p.borderSoft = hex("#E4EBE0");
    p.text = hex("#1C271E");
    p.muted = hex("#56665A");
    p.accent = hex("#1E6B3A");
    p.accentSoft = hex("#DCEBDD");
    p.accentText = hex("#17552E");
    p.marker = hex("#66766A");
    p.gutter = hex("#8C9A8E");
    p.gutterActive = hex("#1C271E");
    p.lineActive = hex("#EEF4EC");
    p.link = hex("#1E6B3A");
    p.tag = hex("#7A5607");
    p.tagBg = hex("#F4EBD3");
    p.codeBg = hex("#EDF1EA");
    p.kw = hex("#6A3FA0");
    p.ok = hex("#2E7D47");

    p.card = hex("#FFFFFF");
    p.onAccent = hex("#FFFFFF");
    p.onAccentMuted = hex("#D5EBDB");
    p.accentHover = hex("#14502A");
    p.hover = hex("#E2EBDE");
    p.bodyText = hex("#3C4A3F");
    p.scrim = hex("#C9D1C5");
    p.danger = hex("#9A3412");
    p.dangerLine = hex("#B03A0B");
    p.dangerBg = hex("#FBE9E1");
    p.warnBg = hex("#FDF3DC");
    p.warnLine = hex("#EBD9A8");
    p.warnButtonLine = hex("#C9B07A");
    p.warnText = hex("#5C4105");
    p.warnIcon = hex("#7A5607");

    p.scroll = hex("#C3CFBE");
    p.scrollHover = hex("#A9B8A4");
    p.dark = false;
    return p;
}

Palette makeDark()
{
    Palette p;
    p.bg = hex("#141A16");
    p.chrome = hex("#0F1411");
    p.panel = hex("#1A221C");
    p.surface = hex("#161D18");
    p.border = hex("#2A352D");
    p.borderSoft = hex("#222C25");
    p.text = hex("#E3EAE2");
    p.muted = hex("#9AAA9C");
    p.accent = hex("#5DBB7A");
    p.accentSoft = hex("#22382A");
    p.accentText = hex("#8FD9A6");
    p.marker = hex("#7F9183");
    p.gutter = hex("#617265");
    p.gutterActive = hex("#E3EAE2");
    p.lineActive = hex("#1C251F");
    p.link = hex("#6FCB8B");
    p.tag = hex("#E7C068");
    p.tagBg = hex("#352B17");
    p.codeBg = hex("#1F2822");
    p.kw = hex("#C3A2F0");
    p.ok = hex("#5DBB7A");

    p.card = hex("#1A221C");
    p.onAccent = hex("#0B1A10");
    p.onAccentMuted = hex("#1D4B2D");
    p.accentHover = hex("#7BCB93");
    p.hover = hex("#222C25");
    p.bodyText = hex("#C3CFC4");
    p.scrim = hex("#0A0E0B");
    p.danger = hex("#F0916B");
    p.dangerLine = hex("#E0693A");
    p.dangerBg = hex("#3A2118");
    p.warnBg = hex("#33290F");
    p.warnLine = hex("#5A4718");
    p.warnButtonLine = hex("#7A6226");
    p.warnText = hex("#F0D58E");
    p.warnIcon = hex("#E7C068");

    p.scroll = hex("#2F3C32");
    p.scrollHover = hex("#41544A");
    p.dark = true;
    return p;
}

double channel(int value)
{
    const double c = value / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double luminance(const QColor& c)
{
    return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green()) + 0.0722 * channel(c.blue());
}

} // namespace

const Palette& lightPalette()
{
    static const Palette palette = makeLight();
    return palette;
}

const Palette& darkPalette()
{
    static const Palette palette = makeDark();
    return palette;
}

QList<QPair<QString, QColor>> paletteEntries(const Palette& p)
{
    const auto e = [](const char* name, const QColor& color) {
        return QPair<QString, QColor>(QString::fromLatin1(name), color);
    };
    return {e("bg", p.bg),
            e("chrome", p.chrome),
            e("panel", p.panel),
            e("surface", p.surface),
            e("border", p.border),
            e("borderSoft", p.borderSoft),
            e("text", p.text),
            e("muted", p.muted),
            e("accent", p.accent),
            e("accentSoft", p.accentSoft),
            e("accentText", p.accentText),
            e("marker", p.marker),
            e("gutter", p.gutter),
            e("gutterActive", p.gutterActive),
            e("lineActive", p.lineActive),
            e("link", p.link),
            e("tag", p.tag),
            e("tagBg", p.tagBg),
            e("codeBg", p.codeBg),
            e("kw", p.kw),
            e("ok", p.ok),
            e("card", p.card),
            e("onAccent", p.onAccent),
            e("onAccentMuted", p.onAccentMuted),
            e("accentHover", p.accentHover),
            e("hover", p.hover),
            e("bodyText", p.bodyText),
            e("scrim", p.scrim),
            e("danger", p.danger),
            e("dangerLine", p.dangerLine),
            e("dangerBg", p.dangerBg),
            e("warnBg", p.warnBg),
            e("warnLine", p.warnLine),
            e("warnButtonLine", p.warnButtonLine),
            e("warnText", p.warnText),
            e("warnIcon", p.warnIcon),
            e("scroll", p.scroll),
            e("scrollHover", p.scrollHover)};
}

double contrastRatio(const QColor& a, const QColor& b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

QPalette buildQPalette(const Palette& p)
{
    QPalette q;
    q.setColor(QPalette::Window, p.bg);
    q.setColor(QPalette::WindowText, p.text);
    q.setColor(QPalette::Base, p.surface);
    q.setColor(QPalette::AlternateBase, p.panel);
    q.setColor(QPalette::Text, p.text);
    q.setColor(QPalette::Button, p.card);
    q.setColor(QPalette::ButtonText, p.text);
    q.setColor(QPalette::BrightText, p.onAccent);
    q.setColor(QPalette::ToolTipBase, p.card);
    q.setColor(QPalette::ToolTipText, p.text);
    q.setColor(QPalette::PlaceholderText, p.gutter);
    q.setColor(QPalette::Highlight, p.accentSoft);
    q.setColor(QPalette::HighlightedText, p.accentText);
    q.setColor(QPalette::Link, p.link);
    q.setColor(QPalette::LinkVisited, p.link);
    q.setColor(QPalette::Light, p.card);
    q.setColor(QPalette::Midlight, p.borderSoft);
    q.setColor(QPalette::Mid, p.border);
    q.setColor(QPalette::Dark, p.border);
    q.setColor(QPalette::Shadow, p.border);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        q.setColor(QPalette::Disabled, role, p.gutter);
    }
    return q;
}

QString buildStyleSheet(const QString& templateText, const Palette& palette)
{
    QString result = templateText;
    for (const auto& [name, color] : paletteEntries(palette)) {
        result.replace(QLatin1Char('@') + name + QLatin1Char('@'), color.name(QColor::HexRgb));
    }
    result.replace(QStringLiteral("@checkImage@"), palette.dark
                                                       ? QStringLiteral("check-on-dark.svg")
                                                       : QStringLiteral("check-on-light.svg"));
    return result;
}

} // namespace nexus::ui
