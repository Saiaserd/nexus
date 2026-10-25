#include <gtest/gtest.h>

#include <QString>

#include "core/PathOrder.h"

using nexus::core::isSafeRelPath;
using nexus::core::naturalLess;
using nexus::core::pathLess;

TEST(PathOrderTest, NaturalOrderComparesNumbersByValue)
{
    EXPECT_TRUE(naturalLess(QStringLiteral("筆記2"), QStringLiteral("筆記10")));
    EXPECT_FALSE(naturalLess(QStringLiteral("筆記10"), QStringLiteral("筆記2")));
    EXPECT_TRUE(naturalLess(QStringLiteral("a9"), QStringLiteral("a10")));
    EXPECT_TRUE(naturalLess(QStringLiteral("a1"),
                            QStringLiteral("a01x"))); // equal value, then case-sensitive tie-break
}

TEST(PathOrderTest, NaturalOrderIgnoresCaseButIsTotal)
{
    EXPECT_TRUE(naturalLess(QStringLiteral("apple"), QStringLiteral("Banana")));
    EXPECT_FALSE(naturalLess(QStringLiteral("Banana"), QStringLiteral("apple")));
    // Same letters, different case: exactly one direction is true.
    EXPECT_NE(naturalLess(QStringLiteral("a"), QStringLiteral("A")),
              naturalLess(QStringLiteral("A"), QStringLiteral("a")));
    EXPECT_FALSE(naturalLess(QStringLiteral("same"), QStringLiteral("same")));
}

TEST(PathOrderTest, NaturalOrderPrefixSortsFirst)
{
    EXPECT_TRUE(naturalLess(QStringLiteral("note"), QStringLiteral("note1")));
    EXPECT_FALSE(naturalLess(QStringLiteral("note1"), QStringLiteral("note")));
}

TEST(PathOrderTest, FoldersComeBeforeFilesAtTheSameLevel)
{
    // "z/a.md" lives in a folder, "a.md" is a file at the root: folder content first.
    EXPECT_TRUE(pathLess(QStringLiteral("z/a.md"), false, QStringLiteral("a.md"), false));
    EXPECT_FALSE(pathLess(QStringLiteral("a.md"), false, QStringLiteral("z/a.md"), false));
}

TEST(PathOrderTest, ParentComesBeforeChildren)
{
    EXPECT_TRUE(pathLess(QStringLiteral("dir"), true, QStringLiteral("dir/sub"), true));
    EXPECT_FALSE(pathLess(QStringLiteral("dir/sub"), true, QStringLiteral("dir"), true));
}

TEST(PathOrderTest, SiblingFoldersUseNaturalOrder)
{
    EXPECT_TRUE(pathLess(QStringLiteral("資料夾2"), true, QStringLiteral("資料夾10"), true));
    EXPECT_FALSE(pathLess(QStringLiteral("x"), true, QStringLiteral("x"), true));
}

TEST(PathOrderTest, SafeRelPath)
{
    EXPECT_TRUE(isSafeRelPath(QStringLiteral("a.md")));
    EXPECT_TRUE(isSafeRelPath(QStringLiteral("dir/筆記 1.md")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("/abs.md")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("a/../b.md")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("../b.md")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("a//b.md")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("a/")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("a\\b.md")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("C:/x.md")));
    EXPECT_FALSE(isSafeRelPath(QStringLiteral("./a.md")));
}
