#include <gtest/gtest.h>

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>

#include "ui/Fonts.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/ThemeManager.h"

using nexus::ui::contrastRatio;
using nexus::ui::Palette;
using nexus::ui::ThemeManager;
using nexus::ui::ThemeMode;

namespace {

// Colours of one `const light = {...}` / `const dark = {...}` block in the design file.
QHash<QString, QString> designColors(const QString& block)
{
    QFile file(QStringLiteral(NEXUS_SOURCE_DIR "/docs/design/m1/Main.dc.html"));
    EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << "design file not found";
    const QString html = QString::fromUtf8(file.readAll());

    const qsizetype start = html.indexOf(QStringLiteral("const %1 = {").arg(block));
    const qsizetype end = html.indexOf(QStringLiteral("};"), start);
    EXPECT_GE(start, 0);
    EXPECT_GT(end, start);

    QHash<QString, QString> result;
    static const QRegularExpression entry(QStringLiteral("(\\w+):\\s*'(#[0-9A-Fa-f]{6})'"));
    auto it = entry.globalMatch(html.mid(start, end - start));
    while (it.hasNext()) {
        const auto match = it.next();
        result.insert(match.captured(1), match.captured(2).toUpper());
    }
    return result;
}

QString hexOf(const QColor& color)
{
    return color.name(QColor::HexRgb).toUpper();
}

void expectMatchesDesign(const Palette& palette, const QString& block)
{
    const QHash<QString, QString> design = designColors(block);
    EXPECT_EQ(design.size(), 21) << "design palette has 21 colours";

    QHash<QString, QString> ours;
    for (const auto& [name, color] : nexus::ui::paletteEntries(palette)) {
        ours.insert(name, hexOf(color));
    }
    for (auto it = design.cbegin(); it != design.cend(); ++it) {
        ASSERT_TRUE(ours.contains(it.key())) << qPrintable(it.key());
        EXPECT_EQ(ours.value(it.key()), it.value()) << qPrintable(it.key());
    }
}

QString templateText()
{
    QFile file(QStringLiteral(":/ui/app.qss.in"));
    EXPECT_TRUE(file.open(QIODevice::ReadOnly));
    return QString::fromUtf8(file.readAll());
}

} // namespace

TEST(Palette, LightMatchesDesignFile)
{
    expectMatchesDesign(nexus::ui::lightPalette(), QStringLiteral("light"));
}

TEST(Palette, DarkMatchesDesignFile)
{
    expectMatchesDesign(nexus::ui::darkPalette(), QStringLiteral("dark"));
}

TEST(Palette, LightExtrasMatchWelcomeAndStatesMockups)
{
    const Palette& p = nexus::ui::lightPalette();
    EXPECT_EQ(hexOf(p.card), "#FFFFFF");
    EXPECT_EQ(hexOf(p.onAccentMuted), "#D5EBDB");
    EXPECT_EQ(hexOf(p.accentHover), "#14502A");
    EXPECT_EQ(hexOf(p.hover), "#E2EBDE");
    EXPECT_EQ(hexOf(p.bodyText), "#3C4A3F");
    EXPECT_EQ(hexOf(p.scrim), "#C9D1C5");
    EXPECT_EQ(hexOf(p.danger), "#9A3412");
    EXPECT_EQ(hexOf(p.dangerLine), "#B03A0B");
    EXPECT_EQ(hexOf(p.dangerBg), "#FBE9E1");
    EXPECT_EQ(hexOf(p.warnBg), "#FDF3DC");
    EXPECT_EQ(hexOf(p.warnLine), "#EBD9A8");
    EXPECT_EQ(hexOf(p.warnButtonLine), "#C9B07A");
    EXPECT_EQ(hexOf(p.warnText), "#5C4105");
    EXPECT_EQ(hexOf(p.warnIcon), "#7A5607");
}

TEST(Palette, EveryEntryIsAValidColour)
{
    for (const Palette* p : {&nexus::ui::lightPalette(), &nexus::ui::darkPalette()}) {
        for (const auto& [name, color] : nexus::ui::paletteEntries(*p)) {
            EXPECT_TRUE(color.isValid()) << qPrintable(name);
        }
    }
}

TEST(Contrast, KnownRatios)
{
    EXPECT_NEAR(contrastRatio(Qt::black, Qt::white), 21.0, 0.01);
    EXPECT_NEAR(contrastRatio(Qt::white, Qt::white), 1.0, 0.001);
}

TEST(Contrast, TextAndMutedReadableOnEverySurface)
{
    for (const Palette* p : {&nexus::ui::lightPalette(), &nexus::ui::darkPalette()}) {
        for (const QColor& fg : {p->text, p->muted}) {
            for (const QColor& bg : {p->bg, p->panel, p->surface, p->chrome, p->card}) {
                EXPECT_GE(contrastRatio(fg, bg), 4.5)
                    << qPrintable(hexOf(fg)) << " on " << qPrintable(hexOf(bg));
            }
        }
    }
}

TEST(Contrast, AccentAndStatusTextsReadable)
{
    for (const Palette* p : {&nexus::ui::lightPalette(), &nexus::ui::darkPalette()}) {
        EXPECT_GE(contrastRatio(p->accentText, p->accentSoft), 4.5);
        EXPECT_GE(contrastRatio(p->onAccent, p->accent), 4.5);
        EXPECT_GE(contrastRatio(p->danger, p->card), 4.5);
        EXPECT_GE(contrastRatio(p->danger, p->dangerBg), 4.5);
        EXPECT_GE(contrastRatio(p->warnText, p->warnBg), 4.5);
        EXPECT_GE(contrastRatio(p->link, p->surface), 4.5);
    }
}

TEST(StyleSheet, TemplateExpandsEveryToken)
{
    const QString text = templateText();
    ASSERT_FALSE(text.isEmpty());

    for (const Palette* p : {&nexus::ui::lightPalette(), &nexus::ui::darkPalette()}) {
        const QString qss = nexus::ui::buildStyleSheet(text, *p);
        EXPECT_FALSE(qss.contains(QLatin1Char('@'))) << "unreplaced token";
        EXPECT_TRUE(qss.contains(hexOf(p->chrome), Qt::CaseInsensitive));
        EXPECT_TRUE(qss.contains(p->dark ? "check-on-dark" : "check-on-light"));
    }
}

TEST(StyleSheet, TemplateHasNoHardCodedColours)
{
    static const QRegularExpression literal(QStringLiteral("#[0-9A-Fa-f]{3,8}\\b|rgba?\\("));
    EXPECT_FALSE(literal.match(templateText()).hasMatch());
}

// NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
class ThemeManagerTest : public ::testing::Test {
protected:
    [[nodiscard]] QString iniPath() const { return m_dir.filePath(QStringLiteral("t.ini")); }

private:
    QTemporaryDir m_dir;
};
// NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

TEST_F(ThemeManagerTest, DefaultsToLight)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    const ThemeManager theme(settings);
    EXPECT_EQ(theme.mode(), ThemeMode::Light);
    EXPECT_FALSE(theme.isDark());
}

TEST_F(ThemeManagerTest, SwitchingAppliesImmediatelyAndNotifies)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    ThemeManager theme(settings);
    int changes = 0;
    QObject::connect(&theme, &ThemeManager::themeChanged, &theme, [&changes] { ++changes; });

    theme.setMode(ThemeMode::Dark);
    EXPECT_TRUE(theme.isDark());
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(QApplication::palette().color(QPalette::Window), nexus::ui::darkPalette().bg);
    EXPECT_TRUE(
        qApp->styleSheet().contains(hexOf(nexus::ui::darkPalette().chrome), Qt::CaseInsensitive));

    theme.setMode(ThemeMode::Light);
    EXPECT_FALSE(theme.isDark());
    EXPECT_EQ(changes, 2);
    EXPECT_EQ(QApplication::palette().color(QPalette::Window), nexus::ui::lightPalette().bg);

    theme.setMode(ThemeMode::Light); // no change, no signal
    EXPECT_EQ(changes, 2);
}

TEST_F(ThemeManagerTest, ChoiceIsRemembered)
{
    {
        QSettings settings(iniPath(), QSettings::IniFormat);
        ThemeManager theme(settings);
        theme.setMode(ThemeMode::Dark);
    }
    QSettings settings(iniPath(), QSettings::IniFormat);
    const ThemeManager theme(settings);
    EXPECT_EQ(theme.mode(), ThemeMode::Dark);
    EXPECT_TRUE(theme.isDark());
}

TEST_F(ThemeManagerTest, NonPersistentOverrideIsNotSaved)
{
    {
        QSettings settings(iniPath(), QSettings::IniFormat);
        ThemeManager theme(settings);
        theme.setMode(ThemeMode::Light);
        theme.setMode(ThemeMode::Dark, /*persist=*/false);
        EXPECT_TRUE(theme.isDark());
    }
    QSettings settings(iniPath(), QSettings::IniFormat);
    const ThemeManager theme(settings);
    EXPECT_EQ(theme.mode(), ThemeMode::Light);
}

TEST_F(ThemeManagerTest, FollowSystemTracksOperatingSystem)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    ThemeManager theme(settings);
    theme.setMode(ThemeMode::System);
    int changes = 0;
    QObject::connect(&theme, &ThemeManager::themeChanged, &theme, [&changes] { ++changes; });

    theme.handleSystemSchemeChanged(Qt::ColorScheme::Dark);
    EXPECT_TRUE(theme.isDark());
    EXPECT_EQ(QApplication::palette().color(QPalette::Window), nexus::ui::darkPalette().bg);
    EXPECT_EQ(changes, 1);

    theme.handleSystemSchemeChanged(Qt::ColorScheme::Dark); // same again: nothing to do
    EXPECT_EQ(changes, 1);

    theme.handleSystemSchemeChanged(Qt::ColorScheme::Light);
    EXPECT_FALSE(theme.isDark());
    EXPECT_EQ(changes, 2);
}

TEST_F(ThemeManagerTest, ExplicitChoiceIgnoresOperatingSystem)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    ThemeManager theme(settings);
    theme.setMode(ThemeMode::Light);
    theme.handleSystemSchemeChanged(Qt::ColorScheme::Dark);
    EXPECT_FALSE(theme.isDark());

    theme.setMode(ThemeMode::Dark);
    theme.handleSystemSchemeChanged(Qt::ColorScheme::Light);
    EXPECT_TRUE(theme.isDark());
}

TEST(ThemeMode, StringRoundTrip)
{
    for (const ThemeMode mode : {ThemeMode::Light, ThemeMode::Dark, ThemeMode::System}) {
        EXPECT_EQ(ThemeManager::fromString(ThemeManager::toString(mode)), mode);
    }
    EXPECT_EQ(ThemeManager::fromString(QStringLiteral("garbage")), ThemeMode::Light);
}

TEST(Fonts, EmbeddedMonospaceIsLoaded)
{
    nexus::ui::fonts::install();
    EXPECT_EQ(nexus::ui::fonts::monoFamily(), QStringLiteral("JetBrains Mono"));
    EXPECT_EQ(nexus::ui::fonts::monoFont(13).pixelSize(), 13);
}

TEST(Icons, TintedWithRequestedColour)
{
    const QColor tint(0x1E, 0x6B, 0x3A);
    const QImage image =
        nexus::ui::icons::pixmap(QStringLiteral("folder"), tint, 16, 1.0).toImage();
    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(image.width(), 16);

    bool found = false;
    for (int y = 0; y < image.height() && !found; ++y) {
        for (int x = 0; x < image.width() && !found; ++x) {
            const QColor c = image.pixelColor(x, y);
            found = c.alpha() > 200 && c.red() == tint.red() && c.green() == tint.green();
        }
    }
    EXPECT_TRUE(found);
}

TEST(Icons, UnknownNameGivesNullPixmap)
{
    EXPECT_TRUE(nexus::ui::icons::pixmap(QStringLiteral("nope"), Qt::black, 16, 1.0).isNull());
}

TEST(Icons, EveryBundledIconRenders)
{
    for (const char* name :
         {"logo", "folder", "folder-plus", "file", "file-plus", "collapse-all", "chevron-down",
          "chevron-right", "close", "alert-circle", "alert-triangle", "refresh", "check"}) {
        EXPECT_FALSE(nexus::ui::icons::icon(QString::fromLatin1(name), Qt::black, 16).isNull())
            << name;
    }
}

// Verifies the crash-dialog suppression: a qFatal must terminate with a non-zero status
// instead of blocking on a modal "Debug Error!" box (a hang would fail by test timeout).
TEST(CrashDialogsDeathTest, QFatalDiesWithoutDialog)
{
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_DEATH(qFatal("intentional test failure"), "intentional test failure");
}
