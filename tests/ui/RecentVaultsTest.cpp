#include <gtest/gtest.h>

#include <QDateTime>
#include <QSettings>
#include <QTemporaryDir>

#include "ui/RecentVaults.h"
#include "ui/UiText.h"

using nexus::ui::formatRelativeTime;
using nexus::ui::RecentVaults;

namespace {

// NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
class RecentVaultsTest : public ::testing::Test {
protected:
    [[nodiscard]] QString iniPath() const { return m_dir.filePath(QStringLiteral("r.ini")); }
    [[nodiscard]] QString folder(const QString& name) const { return m_dir.filePath(name); }

private:
    QTemporaryDir m_dir;
};
// NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

QStringList paths(const RecentVaults& recent)
{
    QStringList result;
    for (const auto& entry : recent.entries()) {
        result << entry.path;
    }
    return result;
}

} // namespace

TEST_F(RecentVaultsTest, StartsEmpty)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    const RecentVaults recent(settings);
    EXPECT_TRUE(recent.entries().isEmpty());
    EXPECT_TRUE(recent.lastOpenVault().isEmpty());
}

TEST_F(RecentVaultsTest, NewestFirst)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    RecentVaults recent(settings);
    recent.add(folder(QStringLiteral("a")));
    recent.add(folder(QStringLiteral("b")));
    recent.add(folder(QStringLiteral("c")));
    EXPECT_EQ(paths(recent),
              (QStringList{RecentVaults::normalizedPath(folder(QStringLiteral("c"))),
                           RecentVaults::normalizedPath(folder(QStringLiteral("b"))),
                           RecentVaults::normalizedPath(folder(QStringLiteral("a")))}));
}

TEST_F(RecentVaultsTest, ReopeningMovesToTopWithoutDuplicate)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    RecentVaults recent(settings);
    recent.add(folder(QStringLiteral("a")));
    recent.add(folder(QStringLiteral("b")));
    recent.add(folder(QStringLiteral("a")) + QStringLiteral("/")); // same folder, trailing slash
    ASSERT_EQ(recent.entries().size(), 2);
    EXPECT_EQ(recent.entries().first().path,
              RecentVaults::normalizedPath(folder(QStringLiteral("a"))));
}

#ifdef Q_OS_WIN
TEST_F(RecentVaultsTest, WindowsPathsAreCaseInsensitive)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    RecentVaults recent(settings);
    recent.add(folder(QStringLiteral("Notes")));
    recent.add(folder(QStringLiteral("notes")));
    EXPECT_EQ(recent.entries().size(), 1);
}
#endif

TEST_F(RecentVaultsTest, KeepsAtMostTenEntries)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    RecentVaults recent(settings);
    for (int i = 0; i < 15; ++i) {
        recent.add(folder(QStringLiteral("v%1").arg(i)));
    }
    const auto entries = recent.entries();
    ASSERT_EQ(entries.size(), RecentVaults::kMaxEntries);
    EXPECT_EQ(entries.first().path, RecentVaults::normalizedPath(folder(QStringLiteral("v14"))));
    EXPECT_EQ(entries.last().path, RecentVaults::normalizedPath(folder(QStringLiteral("v5"))));
}

TEST_F(RecentVaultsTest, RemoveAndClear)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    RecentVaults recent(settings);
    recent.add(folder(QStringLiteral("a")));
    recent.add(folder(QStringLiteral("b")));
    recent.remove(folder(QStringLiteral("a")));
    ASSERT_EQ(recent.entries().size(), 1);
    EXPECT_EQ(recent.entries().first().path,
              RecentVaults::normalizedPath(folder(QStringLiteral("b"))));
    recent.remove(folder(QStringLiteral("not-in-list"))); // harmless
    EXPECT_EQ(recent.entries().size(), 1);
    recent.clear();
    EXPECT_TRUE(recent.entries().isEmpty());
}

TEST_F(RecentVaultsTest, SurvivesRestartWithTimestamp)
{
    const QDateTime when(QDate(2026, 10, 1), QTime(8, 30, 15));
    {
        QSettings settings(iniPath(), QSettings::IniFormat);
        RecentVaults recent(settings);
        recent.add(folder(QStringLiteral("a")), when);
    }
    QSettings settings(iniPath(), QSettings::IniFormat);
    const RecentVaults recent(settings);
    ASSERT_EQ(recent.entries().size(), 1);
    EXPECT_EQ(recent.entries().first().opened, when);
}

TEST_F(RecentVaultsTest, NonAsciiAndSpacesInPaths)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    RecentVaults recent(settings);
    const QString path = folder(QStringLiteral("我的 筆記, \"引號\""));
    recent.add(path);
    ASSERT_EQ(recent.entries().size(), 1);
    EXPECT_EQ(recent.entries().first().path, RecentVaults::normalizedPath(path));
}

TEST_F(RecentVaultsTest, CorruptStoredValueIsIgnored)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("recent/vaults"), QStringLiteral("not json {{"));
    const RecentVaults recent(settings);
    EXPECT_TRUE(recent.entries().isEmpty());
}

TEST_F(RecentVaultsTest, LastOpenVaultCanBeSetAndCleared)
{
    QSettings settings(iniPath(), QSettings::IniFormat);
    RecentVaults recent(settings);
    recent.setLastOpenVault(folder(QStringLiteral("a")));
    EXPECT_EQ(recent.lastOpenVault(), RecentVaults::normalizedPath(folder(QStringLiteral("a"))));
    recent.setLastOpenVault(QString());
    EXPECT_TRUE(recent.lastOpenVault().isEmpty());
}

TEST(RecentVaultsNames, DisplayNameIsTheFolderName)
{
    EXPECT_EQ(RecentVaults::displayName(QStringLiteral("C:/Users/me/notes")),
              QStringLiteral("notes"));
    EXPECT_EQ(RecentVaults::displayName(QStringLiteral("C:/Users/me/notes/")),
              QStringLiteral("notes"));
    EXPECT_EQ(RecentVaults::displayName(QStringLiteral("C:/Users/me/nexus-規格")),
              QStringLiteral("nexus-規格"));
    EXPECT_EQ(RecentVaults::displayName(QString()), QString());
}

TEST(RelativeTime, Buckets)
{
    const QDateTime now(QDate(2026, 10, 25), QTime(15, 0));
    const auto ago = [&now](int days) {
        return formatRelativeTime(now.addDays(-days).addSecs(-3600), now);
    };
    EXPECT_EQ(formatRelativeTime(now.addSecs(-60), now), QStringLiteral("今天"));
    EXPECT_EQ(ago(0), QStringLiteral("今天"));
    EXPECT_EQ(ago(1), QStringLiteral("昨天"));
    EXPECT_EQ(ago(2), QStringLiteral("2 天前"));
    EXPECT_EQ(ago(3), QStringLiteral("3 天前"));
    EXPECT_EQ(ago(6), QStringLiteral("6 天前"));
    EXPECT_EQ(ago(7), QStringLiteral("1 週前"));
    EXPECT_EQ(ago(20), QStringLiteral("2 週前"));
    EXPECT_EQ(ago(30), QStringLiteral("1 個月前"));
    EXPECT_EQ(ago(200), QStringLiteral("6 個月前"));
    EXPECT_EQ(ago(365), QStringLiteral("1 年前"));
    EXPECT_EQ(ago(800), QStringLiteral("2 年前"));
}

TEST(RelativeTime, UsesCalendarDaysNotHours)
{
    const QDateTime now(QDate(2026, 10, 25), QTime(0, 5));
    EXPECT_EQ(formatRelativeTime(QDateTime(QDate(2026, 10, 24), QTime(23, 55)), now),
              QStringLiteral("昨天"));
}

TEST(RelativeTime, InvalidAndFuture)
{
    const QDateTime now(QDate(2026, 10, 25), QTime(15, 0));
    EXPECT_TRUE(formatRelativeTime(QDateTime(), now).isEmpty());
    EXPECT_EQ(formatRelativeTime(now.addDays(3), now), QStringLiteral("2026-10-28"));
}

TEST(WindowTitle, Parts)
{
    using nexus::ui::formatWindowTitle;
    EXPECT_EQ(formatWindowTitle(QStringLiteral("vault-管理"), QStringLiteral("nexus-規格")),
              QStringLiteral("vault-管理 — nexus-規格 — Nexus"));
    EXPECT_EQ(formatWindowTitle(QString(), QStringLiteral("nexus-規格")),
              QStringLiteral("nexus-規格 — Nexus"));
    EXPECT_EQ(formatWindowTitle(QString(), QString()), QStringLiteral("Nexus"));
}

TEST(OpenErrorText, MentionsTheFolderAndTheReason)
{
    using nexus::core::FileErrorKind;
    using nexus::ui::describeOpenError;
    using nexus::ui::VaultOpenError;

    const auto text = [](bool notEmpty, FileErrorKind kind) {
        return describeOpenError(VaultOpenError{notEmpty, {kind, QStringLiteral("C:/x"), {}}});
    };
    EXPECT_TRUE(text(false, FileErrorKind::NotFound).contains(QStringLiteral("找不到")));
    EXPECT_TRUE(text(false, FileErrorKind::NotADirectory).contains(QStringLiteral("不是資料夾")));
    EXPECT_TRUE(text(false, FileErrorKind::AccessDenied).contains(QStringLiteral("權限")));
    EXPECT_TRUE(text(true, FileErrorKind::AlreadyExists).contains(QStringLiteral("空資料夾")));
    EXPECT_TRUE(text(false, FileErrorKind::IoError).contains(QStringLiteral("x")));
}
