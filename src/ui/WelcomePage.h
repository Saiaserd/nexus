#pragma once

#include <functional>

#include <QFutureWatcher>
#include <QList>
#include <QSet>
#include <QWidget>

#include "ui/RecentVaults.h"

class QBoxLayout;
class QFrame;
class QLabel;

namespace nexus::ui {

class ClickableFrame;
class ThemeManager;

// The page shown while no vault is open (design: Welcome.dc.html): product name, the two big
// actions, the recently opened vaults and a footer line. Whether the recent folders still exist
// is checked on a worker thread, so a dead network drive cannot freeze the page; rows flip to
// the red "folder not found" state when the answer arrives.
class WelcomePage : public QWidget {
    Q_OBJECT

public:
    using ExistsCheck = std::function<bool(const QString& path)>;

    explicit WelcomePage(ThemeManager& theme, QWidget* parent = nullptr);
    ~WelcomePage() override;

    WelcomePage(const WelcomePage&) = delete;
    WelcomePage& operator=(const WelcomePage&) = delete;
    WelcomePage(WelcomePage&&) = delete;
    WelcomePage& operator=(WelcomePage&&) = delete;

    // Replaces the list and starts the background existence check.
    void setRecent(const QList<RecentVault>& entries);
    // The check used for the background test; the default is QFileInfo::isDir. Runs on a
    // worker thread. Set before setRecent().
    void setExistsCheck(ExistsCheck check) { m_exists = std::move(check); }
    // While a vault is opening the two actions and the rows ignore input.
    void setBusy(bool busy);

    [[nodiscard]] int rowCount() const { return static_cast<int>(m_entries.size()); }
    [[nodiscard]] bool isMissing(const QString& path) const { return m_missing.contains(path); }
    [[nodiscard]] bool checkFinished() const { return !m_checking; }

signals:
    void openFolderRequested();
    void createVaultRequested();
    void recentActivated(const QString& path);
    void removeRecentRequested(const QString& path);
    void clearRecentRequested();
    void quickStartRequested();
    void appearanceRequested();
    // Emitted when the background check has updated the rows.
    void checkCompleted();

private:
    void build();
    void rebuildRows();
    void applyIcons();
    void refreshFooter();
    void onCheckFinished();

    ThemeManager& m_theme;
    ExistsCheck m_exists;

    ClickableFrame* m_openCard = nullptr;
    ClickableFrame* m_createCard = nullptr;
    QLabel* m_logo = nullptr;
    QLabel* m_openIcon = nullptr;
    QLabel* m_createIcon = nullptr;
    QLabel* m_footer = nullptr;
    QWidget* m_recentSection = nullptr;
    QFrame* m_list = nullptr;
    QBoxLayout* m_listLayout = nullptr;

    QList<RecentVault> m_entries;
    QSet<QString> m_missing;
    bool m_checking = false;
    bool m_busy = false;
    quint64 m_generation = 0;
    QFutureWatcher<QList<bool>>* m_watcher = nullptr;
};

} // namespace nexus::ui
