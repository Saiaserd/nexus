#pragma once

#include <QDateTime>
#include <QList>
#include <QSettings>
#include <QString>

namespace nexus::ui {

struct RecentVault {
    QString path; // absolute, cleaned, '/' separated
    QDateTime opened;

    friend bool operator==(const RecentVault& a, const RecentVault& b)
    {
        return a.path == b.path && a.opened == b.opened;
    }
};

// The "recently opened" list and the vault to reopen at start-up, kept in QSettings (JSON text,
// so it is easy to read and edit). Never touches the disk: whether a folder still exists is
// checked in the background by the welcome page (a network drive could block).
class RecentVaults {
public:
    static constexpr int kMaxEntries = 10;

    // `settings` must outlive this object.
    explicit RecentVaults(QSettings& settings);

    // Newest first, at most kMaxEntries.
    [[nodiscard]] QList<RecentVault> entries() const;
    // Puts the vault at the top (de-duplicated by path) and drops entries beyond the limit.
    void add(const QString& path, const QDateTime& when = QDateTime::currentDateTime());
    void remove(const QString& path);
    void clear();

    // The vault that was open when the application last ran; empty after "close vault".
    [[nodiscard]] QString lastOpenVault() const;
    void setLastOpenVault(const QString& path);

    // Absolute, cleaned, '/' separated; empty for an empty input. No file-system access.
    [[nodiscard]] static QString normalizedPath(const QString& path);
    // Folder name shown as the vault name ("C:/Users/me/notes" -> "notes", "D:/" -> "D:").
    [[nodiscard]] static QString displayName(const QString& path);

private:
    void store(const QList<RecentVault>& list);

    QSettings* m_settings;
};

// "今天", "昨天", "3 天前", "2 週前", "4 個月前", "2 年前"; empty if `then` is invalid.
[[nodiscard]] QString formatRelativeTime(const QDateTime& then, const QDateTime& now);

} // namespace nexus::ui
