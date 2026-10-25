#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <QDateTime>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include "TestSupport.h"
#include "ui/ClickableFrame.h"
#include "ui/ElidedLabel.h"
#include "ui/RecentVaults.h"
#include "ui/ThemeManager.h"
#include "ui/WelcomePage.h"

using nexus::test::waitFor;
using nexus::ui::RecentVault;
using nexus::ui::ThemeManager;
using nexus::ui::WelcomePage;

namespace {

// NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
class WelcomePageTest : public ::testing::Test {
protected:
    WelcomePageTest()
        : m_settings(m_dir.filePath(QStringLiteral("w.ini")), QSettings::IniFormat),
          m_theme(m_settings)
    {
    }

    [[nodiscard]] static QList<RecentVault> entries(const QStringList& paths)
    {
        QList<RecentVault> list;
        for (const QString& path : paths) {
            list.push_back({path, QDateTime::currentDateTime()});
        }
        return list;
    }

    [[nodiscard]] static QList<QFrame*> rows(const WelcomePage& page)
    {
        return page.findChildren<QFrame*>(QStringLiteral("recentRow"));
    }

    QTemporaryDir m_dir;
    QSettings m_settings;
    ThemeManager m_theme;
};
// NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

} // namespace

TEST_F(WelcomePageTest, HidesRecentSectionWhenListIsEmpty)
{
    WelcomePage page(m_theme);
    page.show();
    page.setRecent({});
    EXPECT_EQ(page.rowCount(), 0);
    EXPECT_TRUE(rows(page).isEmpty());
    EXPECT_FALSE(page.findChild<QWidget*>(QStringLiteral("recentSection"))->isVisible());
}

TEST_F(WelcomePageTest, ShowsOneRowPerEntryWithNameAndPath)
{
    WelcomePage page(m_theme);
    page.setExistsCheck([](const QString&) { return true; });
    page.show();
    page.setRecent(entries({QStringLiteral("C:/notes/alpha"), QStringLiteral("D:/work/中文筆記")}));
    ASSERT_EQ(rows(page).size(), 2);
    EXPECT_TRUE(page.findChild<QWidget*>(QStringLiteral("recentSection"))->isVisible());

    const auto names = page.findChildren<QLabel*>(QStringLiteral("recentName"));
    ASSERT_EQ(names.size(), 2);
    EXPECT_EQ(names.at(0)->text(), QStringLiteral("alpha"));
    EXPECT_EQ(names.at(1)->text(), QStringLiteral("中文筆記"));

    const auto pathLabels =
        page.findChildren<nexus::ui::ElidedLabel*>(QStringLiteral("recentPath"));
    ASSERT_EQ(pathLabels.size(), 2);
    EXPECT_EQ(pathLabels.at(0)->fullText(),
              QDir::toNativeSeparators(QStringLiteral("C:/notes/alpha")));
}

TEST_F(WelcomePageTest, MissingFoldersAreFlaggedAfterTheBackgroundCheck)
{
    QTemporaryDir real;
    const QString good = real.path();
    const QString bad = real.filePath(QStringLiteral("gone"));

    WelcomePage page(m_theme); // default check: QFileInfo::isDir
    page.show();
    page.setRecent(entries({good, bad}));
    ASSERT_TRUE(waitFor([&] { return page.checkFinished(); }));

    EXPECT_FALSE(page.isMissing(good));
    EXPECT_TRUE(page.isMissing(bad));
    int missingRows = 0;
    for (const QFrame* row : rows(page)) {
        if (row->property("missing").toBool()) {
            ++missingRows;
            EXPECT_NE(row->findChild<QPushButton*>(), nullptr) << "needs a remove button";
        }
    }
    EXPECT_EQ(missingRows, 1);
    const auto message = page.findChild<nexus::ui::ElidedLabel*>(QStringLiteral("recentMissing"));
    ASSERT_NE(message, nullptr);
    EXPECT_TRUE(message->fullText().startsWith(QStringLiteral("找不到資料夾：")));
}

TEST_F(WelcomePageTest, SlowDriveDoesNotBlockTheUiThread)
{
    std::atomic<bool> release{false};
    WelcomePage page(m_theme);
    page.setExistsCheck([&release](const QString&) {
        while (!release.load()) { // a network drive that does not answer
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    });
    page.show();

    QElapsedTimer timer;
    timer.start();
    page.setRecent(entries({QStringLiteral("//server/share/notes")}));
    EXPECT_LT(timer.elapsed(), 500) << "setRecent must return while the check is still running";
    EXPECT_FALSE(page.checkFinished());
    EXPECT_EQ(rows(page).size(), 1) << "rows are shown at once, assumed to exist";

    release = true;
    ASSERT_TRUE(waitFor([&] { return page.checkFinished(); }));
    EXPECT_TRUE(page.isMissing(QStringLiteral("//server/share/notes")));
}

TEST_F(WelcomePageTest, AnOutdatedCheckResultIsIgnored)
{
    std::atomic<bool> release{false};
    WelcomePage page(m_theme);
    page.setExistsCheck([&release](const QString& path) {
        if (path.contains(QStringLiteral("slow"))) {
            while (!release.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            return false;
        }
        return true;
    });
    page.show();
    page.setRecent(entries({QStringLiteral("C:/slow")}));
    page.setRecent(entries({QStringLiteral("C:/fast")})); // list replaced before the first answer
    ASSERT_TRUE(waitFor([&] { return page.checkFinished(); }));
    release = true;
    QThread::msleep(100);
    QCoreApplication::processEvents();
    EXPECT_FALSE(page.isMissing(QStringLiteral("C:/slow")));
    EXPECT_FALSE(page.isMissing(QStringLiteral("C:/fast")));
    EXPECT_EQ(page.rowCount(), 1);
}

TEST_F(WelcomePageTest, ClicksReportTheRightRequests)
{
    WelcomePage page(m_theme);
    page.setExistsCheck([](const QString& p) { return !p.contains(QStringLiteral("gone")); });
    page.resize(1000, 800);
    page.show();
    page.setRecent(entries({QStringLiteral("C:/live"), QStringLiteral("C:/gone")}));
    ASSERT_TRUE(waitFor([&] { return page.checkFinished(); }));

    int open = 0;
    int create = 0;
    int clear = 0;
    QStringList activated;
    QStringList removed;
    QObject::connect(&page, &WelcomePage::openFolderRequested, &page, [&] { ++open; });
    QObject::connect(&page, &WelcomePage::createVaultRequested, &page, [&] { ++create; });
    QObject::connect(&page, &WelcomePage::clearRecentRequested, &page, [&] { ++clear; });
    QObject::connect(&page, &WelcomePage::recentActivated, &page,
                     [&](const QString& p) { activated << p; });
    QObject::connect(&page, &WelcomePage::removeRecentRequested, &page,
                     [&](const QString& p) { removed << p; });

    const auto cards = page.findChildren<nexus::ui::ClickableFrame*>(QStringLiteral("card"));
    ASSERT_EQ(cards.size(), 2);
    for (auto* card : cards) {
        QTest::mouseClick(card, Qt::LeftButton, Qt::NoModifier, card->rect().center());
    }
    EXPECT_EQ(open, 1);
    EXPECT_EQ(create, 1);

    for (auto* row : page.findChildren<nexus::ui::ClickableFrame*>(QStringLiteral("recentRow"))) {
        QTest::mouseClick(row, Qt::LeftButton, Qt::NoModifier, row->rect().center());
    }
    EXPECT_EQ(activated, QStringList{QStringLiteral("C:/live")})
        << "missing rows are not clickable";

    for (auto* button : page.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("從清單移除") ||
            button->text() == QStringLiteral("清除清單")) {
            button->click();
        }
    }
    EXPECT_EQ(removed, QStringList{QStringLiteral("C:/gone")});
    EXPECT_EQ(clear, 1);
}

TEST_F(WelcomePageTest, KeyboardActivatesACard)
{
    WelcomePage page(m_theme);
    page.show();
    int open = 0;
    QObject::connect(&page, &WelcomePage::openFolderRequested, &page, [&] { ++open; });
    auto* card = page.findChildren<nexus::ui::ClickableFrame*>(QStringLiteral("card")).first();
    QTest::keyClick(card, Qt::Key_Return);
    QTest::keyClick(card, Qt::Key_Space);
    EXPECT_EQ(open, 2);
}

TEST_F(WelcomePageTest, BusyPageIgnoresTheActions)
{
    WelcomePage page(m_theme);
    page.show();
    int open = 0;
    QObject::connect(&page, &WelcomePage::openFolderRequested, &page, [&] { ++open; });
    page.setBusy(true);
    auto* card = page.findChildren<nexus::ui::ClickableFrame*>(QStringLiteral("card")).first();
    QTest::mouseClick(card, Qt::LeftButton, Qt::NoModifier, card->rect().center());
    EXPECT_EQ(open, 0);
    page.setBusy(false);
    QTest::mouseClick(card, Qt::LeftButton, Qt::NoModifier, card->rect().center());
    EXPECT_EQ(open, 1);
}

TEST_F(WelcomePageTest, SurvivesThemeSwitch)
{
    WelcomePage page(m_theme);
    page.setExistsCheck([](const QString&) { return true; });
    page.show();
    page.setRecent(entries({QStringLiteral("C:/a"), QStringLiteral("C:/b")}));
    m_theme.setMode(nexus::ui::ThemeMode::Dark, false);
    EXPECT_EQ(rows(page).size(), 2);
    m_theme.setMode(nexus::ui::ThemeMode::Light, false);
    EXPECT_EQ(rows(page).size(), 2);
}

TEST(ElidedLabelTest, CutsLongTextAndKeepsFullTextAsToolTip)
{
    nexus::ui::ElidedLabel label;
    label.resize(120, 20);
    const QString text = QStringLiteral("C:/a/very/long/path/that/does/not/fit/in/120/pixels");
    label.setFullText(text);
    EXPECT_EQ(label.toolTip(), text);
    EXPECT_NE(label.elidedText(), text);
    EXPECT_TRUE(label.elidedText().endsWith(QChar(0x2026)));
    EXPECT_EQ(label.minimumSizeHint().width(), 0);

    label.resize(2000, 20);
    EXPECT_EQ(label.elidedText(), text);
}
