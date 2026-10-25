#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

#include "TestSupport.h"
#include "core/Vault.h"

using nexus::core::FileErrorKind;
using nexus::core::Vault;
using nexus::test::makeDir;
using nexus::test::writeFile;

namespace {

QStringList notePaths(const Vault& vault)
{
    QStringList out;
    for (const auto& note : vault.notes()) {
        out << note.relPath;
    }
    return out;
}

QStringList folderPaths(const Vault& vault)
{
    QStringList out;
    for (const auto& folder : vault.folders()) {
        out << folder;
    }
    return out;
}

} // namespace

TEST(VaultTest, FindsNotesRecursivelyWithUnicodeAndSpaces)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString root = dir.path();
    ASSERT_TRUE(writeFile(root, "a.md", "hello"));
    ASSERT_TRUE(writeFile(root, "中文筆記.md"));
    ASSERT_TRUE(writeFile(root, "with space.md"));
    ASSERT_TRUE(writeFile(root, "專案/子 資料夾/深層.md"));
    ASSERT_TRUE(writeFile(root, "readme.txt"));

    const auto vault = Vault::open(root);
    ASSERT_TRUE(vault.has_value());
    EXPECT_EQ(notePaths(*vault),
              (QStringList{"專案/子 資料夾/深層.md", "a.md", "with space.md", "中文筆記.md"}));
    EXPECT_EQ(folderPaths(*vault), (QStringList{"專案", "專案/子 資料夾"}));
    EXPECT_EQ(vault->notes()[1].size, 5);
    EXPECT_TRUE(vault->notes()[1].modified.isValid());
}

TEST(VaultTest, ExtensionIsCaseInsensitive)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.path(), "UPPER.MD"));
    ASSERT_TRUE(writeFile(dir.path(), "mixed.Md"));
    ASSERT_TRUE(writeFile(dir.path(), "notmd.mdx"));
    const auto vault = Vault::open(dir.path());
    ASSERT_TRUE(vault.has_value());
    EXPECT_EQ(notePaths(*vault), (QStringList{"mixed.Md", "UPPER.MD"}));
}

TEST(VaultTest, ListsEmptyFolders)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(makeDir(dir.path(), "empty/nested"));
    const auto vault = Vault::open(dir.path());
    ASSERT_TRUE(vault.has_value());
    EXPECT_TRUE(vault->notes().empty());
    EXPECT_EQ(folderPaths(*vault), (QStringList{"empty", "empty/nested"}));
}

TEST(VaultTest, SkipsHiddenFoldersAndFiles)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString root = dir.path();
    ASSERT_TRUE(writeFile(root, ".git/config.md"));
    ASSERT_TRUE(writeFile(root, ".nexus/index.md"));
    ASSERT_TRUE(writeFile(root, ".obsidian/x.md"));
    ASSERT_TRUE(writeFile(root, ".hidden.md"));
    ASSERT_TRUE(writeFile(root, "visible/.secret.md"));
    ASSERT_TRUE(writeFile(root, "visible/ok.md"));
    const auto vault = Vault::open(root);
    ASSERT_TRUE(vault.has_value());
    EXPECT_EQ(notePaths(*vault), (QStringList{"visible/ok.md"}));
    EXPECT_EQ(folderPaths(*vault), (QStringList{"visible"}));
}

TEST(VaultTest, DoesNotFollowDirectorySymlinks)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString root = dir.path();
    ASSERT_TRUE(writeFile(root, "real/note.md"));
    // Symlink creation needs privileges on Windows; skip when it is not allowed.
    if (!QFile::link(root + "/real", root + "/loop")) {
        GTEST_SKIP() << "cannot create symlinks here";
    }
    // Make a real loop: real/back -> root.
    QFile::link(root, root + "/real/back");
    const auto vault = Vault::open(root);
    ASSERT_TRUE(vault.has_value());
    EXPECT_EQ(notePaths(*vault), (QStringList{"real/note.md"}));
    EXPECT_EQ(folderPaths(*vault), (QStringList{"real"}));
}

TEST(VaultTest, MissingRootIsNotFound)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const auto vault = Vault::open(dir.path() + "/nope");
    ASSERT_FALSE(vault.has_value());
    EXPECT_EQ(vault.error().kind, FileErrorKind::NotFound);
    EXPECT_EQ(Vault::open(QString()).error().kind, FileErrorKind::NotFound);
}

TEST(VaultTest, FileRootIsNotADirectory)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.path(), "file.md"));
    const auto vault = Vault::open(dir.path() + "/file.md");
    ASSERT_FALSE(vault.has_value());
    EXPECT_EQ(vault.error().kind, FileErrorKind::NotADirectory);
}

TEST(VaultTest, UnreadableRootIsAccessDenied)
{
#ifdef Q_OS_WIN
    GTEST_SKIP() << "POSIX permissions are not available on Windows";
#else
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(makeDir(dir.path(), "locked"));
    const QString locked = dir.path() + "/locked";
    QFile::setPermissions(locked, QFileDevice::Permissions());
    if (QFileInfo(locked).isReadable()) {
        QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                          QFileDevice::ExeOwner);
        GTEST_SKIP() << "running with privileges that ignore permissions";
    }
    const auto vault = Vault::open(locked);
    QFile::setPermissions(locked,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    ASSERT_FALSE(vault.has_value());
    EXPECT_EQ(vault.error().kind, FileErrorKind::AccessDenied);
#endif
}

TEST(VaultTest, SortsNaturallyWithFoldersFirst)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString root = dir.path();
    for (const char* name : {"筆記10.md", "筆記2.md", "筆記1.md", "Zeta.md"}) {
        ASSERT_TRUE(writeFile(root, name));
    }
    ASSERT_TRUE(writeFile(root, "資料夾10/x.md"));
    ASSERT_TRUE(writeFile(root, "資料夾2/x.md"));
    const auto vault = Vault::open(root);
    ASSERT_TRUE(vault.has_value());
    EXPECT_EQ(notePaths(*vault), (QStringList{"資料夾2/x.md", "資料夾10/x.md", "Zeta.md",
                                              "筆記1.md", "筆記2.md", "筆記10.md"}));
    EXPECT_EQ(folderPaths(*vault), (QStringList{"資料夾2", "資料夾10"}));
}

TEST(VaultTest, AbsolutePathAndContains)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.path(), "sub/a.md"));
    const auto vault = Vault::open(dir.path());
    ASSERT_TRUE(vault.has_value());

    EXPECT_TRUE(vault->contains("sub/a.md"));
    EXPECT_TRUE(vault->contains("sub"));
    EXPECT_FALSE(vault->contains("sub/b.md"));
    EXPECT_EQ(vault->absolutePath("sub/a.md"), vault->root() + "/sub/a.md");
    EXPECT_TRUE(QFile::exists(vault->absolutePath("sub/a.md")));
    EXPECT_TRUE(vault->absolutePath("../evil.md").isEmpty());
    EXPECT_TRUE(vault->absolutePath("/abs.md").isEmpty());
    EXPECT_TRUE(Vault().absolutePath("a.md").isEmpty());
}

TEST(VaultPerf, OpensTenThousandNotes)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString root = dir.path();
    constexpr int kFolders = 100;
    constexpr int kPerFolder = 100;
    for (int f = 0; f < kFolders; ++f) {
        ASSERT_TRUE(makeDir(root, QString("folder %1").arg(f)));
        for (int n = 0; n < kPerFolder; ++n) {
            ASSERT_TRUE(writeFile(root, QString("folder %1/note %2.md").arg(f).arg(n), "x"));
        }
    }

    const auto start = std::chrono::steady_clock::now();
    const auto vault = Vault::open(root);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ASSERT_TRUE(vault.has_value());
    EXPECT_EQ(vault->notes().size(), static_cast<std::size_t>(kFolders * kPerFolder));
    EXPECT_EQ(vault->folders().size(), static_cast<std::size_t>(kFolders));
    EXPECT_EQ(vault->notes().front().relPath, "folder 0/note 0.md");
    EXPECT_EQ(vault->notes().back().relPath, "folder 99/note 99.md");
#ifdef NEXUS_ENFORCE_PERF
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 1000);
#else
    (void)elapsed; // timing is only enforced in Release builds
#endif
}
