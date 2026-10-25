#include "ui/Fonts.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QStringList>

namespace nexus::ui::fonts {

namespace {

QString g_monoFamily; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

QStringList uiFamilies()
{
#ifdef Q_OS_WIN
    return {QStringLiteral("Segoe UI"), QStringLiteral("Noto Sans TC"),
            QStringLiteral("Microsoft JhengHei UI")};
#else
    return {QStringLiteral("Noto Sans CJK TC"), QStringLiteral("Noto Sans TC")};
#endif
}

} // namespace

void install()
{
    if (!g_monoFamily.isEmpty()) {
        return;
    }

    for (const char* file :
         {":/ui/fonts/JetBrainsMono-Regular.ttf", ":/ui/fonts/JetBrainsMono-Medium.ttf"}) {
        const int id = QFontDatabase::addApplicationFont(QString::fromLatin1(file));
        if (id >= 0 && g_monoFamily.isEmpty()) {
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty()) {
                g_monoFamily = families.first();
            }
        }
    }
    if (g_monoFamily.isEmpty()) {
        g_monoFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }

    // Glyphs a family lacks (Chinese in Segoe UI) come from the next family. The list follows
    // the design (Segoe UI, then Noto Sans TC); Microsoft JhengHei UI is the last resort because
    // Qt renders it without a bold weight on Windows 11.
    const QStringList families = uiFamilies();
    QFont::insertSubstitutions(families.first(), families.mid(1));

    QFont font = QGuiApplication::font();
    font.setFamilies(uiFamilies());
    font.setPixelSize(13); // design: font-size 13px
    QGuiApplication::setFont(font);
}

QString monoFamily()
{
    return g_monoFamily.isEmpty() ? QStringLiteral("monospace") : g_monoFamily;
}

QFont monoFont(int pixelSize, QFont::Weight weight)
{
    QFont font(monoFamily());
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    return font;
}

} // namespace nexus::ui::fonts
