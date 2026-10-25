#include "ui/ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QStyleFactory>
#include <QStyleHints>

#include "ui/Fonts.h"

namespace nexus::ui {

namespace {

constexpr auto kSettingsKey = "appearance/theme";

Qt::ColorScheme normalized(Qt::ColorScheme scheme)
{
    return scheme == Qt::ColorScheme::Dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
}

} // namespace

ThemeManager::ThemeManager(QSettings& settings, QObject* parent)
    : QObject(parent), m_settings(settings)
{
    fonts::install();
    // ADR 0006: Fusion + QPalette + one QSS, identical on Windows and Linux.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFile file(QStringLiteral(":/ui/app.qss.in"));
    if (file.open(QIODevice::ReadOnly)) {
        m_template = QString::fromUtf8(file.readAll());
    }

    m_mode = fromString(m_settings.value(QLatin1StringView(kSettingsKey)).toString());
    // Read the OS scheme before we override it; later changes arrive through the signal.
    m_systemScheme = normalized(QGuiApplication::styleHints()->colorScheme());
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            &ThemeManager::handleSystemSchemeChanged);
    apply();
}

const Palette& ThemeManager::palette() const
{
    switch (m_mode) {
    case ThemeMode::Light:
        return lightPalette();
    case ThemeMode::Dark:
        return darkPalette();
    case ThemeMode::System:
        break;
    }
    return m_systemScheme == Qt::ColorScheme::Dark ? darkPalette() : lightPalette();
}

void ThemeManager::setMode(ThemeMode mode, bool persist)
{
    if (persist) {
        m_settings.setValue(QLatin1StringView(kSettingsKey), toString(mode));
    }
    if (mode == m_mode) {
        return;
    }
    m_mode = mode;
    if (mode == ThemeMode::System) {
        // Release our override first so colorScheme() reports the OS value again.
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
        m_systemScheme = normalized(QGuiApplication::styleHints()->colorScheme());
    }
    apply();
}

void ThemeManager::handleSystemSchemeChanged(Qt::ColorScheme scheme)
{
    if (m_mode != ThemeMode::System) {
        return; // an explicit choice overrides the OS; the signal is our own echo
    }
    const Qt::ColorScheme next = normalized(scheme);
    if (next == m_systemScheme) {
        return;
    }
    m_systemScheme = next;
    apply();
}

QString ThemeManager::toString(ThemeMode mode)
{
    switch (mode) {
    case ThemeMode::Light:
        return QStringLiteral("light");
    case ThemeMode::Dark:
        return QStringLiteral("dark");
    case ThemeMode::System:
        break;
    }
    return QStringLiteral("system");
}

ThemeMode ThemeManager::fromString(const QString& text)
{
    if (text == QLatin1StringView("dark")) {
        return ThemeMode::Dark;
    }
    if (text == QLatin1StringView("system")) {
        return ThemeMode::System;
    }
    return ThemeMode::Light;
}

void ThemeManager::apply()
{
    QStyleHints* hints = QGuiApplication::styleHints();
    switch (m_mode) {
    case ThemeMode::Light:
        hints->setColorScheme(Qt::ColorScheme::Light);
        break;
    case ThemeMode::Dark:
        hints->setColorScheme(Qt::ColorScheme::Dark);
        break;
    case ThemeMode::System:
        hints->setColorScheme(Qt::ColorScheme::Unknown); // give the scheme back to the OS
        break;
    }

    const Palette& p = palette();
    QApplication::setPalette(buildQPalette(p));
    qApp->setStyleSheet(buildStyleSheet(m_template, p));
    emit themeChanged();
}

} // namespace nexus::ui
