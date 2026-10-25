#pragma once

#include <QObject>
#include <QSettings>
#include <cstdint>

#include "ui/Theme.h"

namespace nexus::ui {

enum class ThemeMode : std::uint8_t { Light, Dark, System };

// Owns the current theme: applies the palette and the style sheet to the whole application,
// keeps the native title bar in step (QStyleHints::setColorScheme) and remembers the choice.
// Must be created after QApplication and live on the UI thread.
class ThemeManager : public QObject {
    Q_OBJECT

public:
    // `settings` must outlive the manager. Default mode (nothing stored) is Light.
    explicit ThemeManager(QSettings& settings, QObject* parent = nullptr);

    [[nodiscard]] ThemeMode mode() const { return m_mode; }
    // The palette in effect (for System: follows the operating system).
    [[nodiscard]] const Palette& palette() const;
    [[nodiscard]] bool isDark() const { return palette().dark; }

    // `persist` = false is for command-line overrides that must not change the saved choice.
    void setMode(ThemeMode mode, bool persist = true);

    // Slot for QStyleHints::colorSchemeChanged; public so tests can simulate the OS switching.
    void handleSystemSchemeChanged(Qt::ColorScheme scheme);

    static QString toString(ThemeMode mode);
    static ThemeMode fromString(const QString& text);

signals:
    void themeChanged();

private:
    void apply();

    QSettings& m_settings;
    ThemeMode m_mode = ThemeMode::Light;
    Qt::ColorScheme m_systemScheme = Qt::ColorScheme::Light;
    QString m_template;
};

} // namespace nexus::ui
