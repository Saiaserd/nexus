#include <gtest/gtest.h>

#include <QString>

#include "ui/WordCount.h"

using nexus::ui::countWords;

TEST(WordCountTest, EmptyAndPunctuationOnly)
{
    EXPECT_EQ(countWords(QString()), 0);
    EXPECT_EQ(countWords(QStringLiteral("  \n\t ")), 0);
    EXPECT_EQ(countWords(QStringLiteral("# - ** [[ ]] ，。！")), 0);
}

TEST(WordCountTest, LatinRunsCountOnce)
{
    EXPECT_EQ(countWords(QStringLiteral("hello world")), 2);
    EXPECT_EQ(countWords(QStringLiteral("M1 milestone, v2")), 3);
    EXPECT_EQ(countWords(QStringLiteral("snake_case")), 2); // '_' separates
}

TEST(WordCountTest, EveryCjkCharacterCounts)
{
    EXPECT_EQ(countWords(QStringLiteral("知識圖")), 3);
    EXPECT_EQ(countWords(QStringLiteral("ひらがなカタカナ")), 8);
    EXPECT_EQ(countWords(QStringLiteral("한국어")), 3);
}

TEST(WordCountTest, MixedText)
{
    // 開 啟 一 個 資 料 夾 作 為 + vault + 載 入 + md + 檔
    EXPECT_EQ(countWords(QStringLiteral("開啟一個資料夾作為 vault，載入 .md 檔。")), 14);
    EXPECT_EQ(countWords(QStringLiteral("中文English混合")), 5);
}

TEST(WordCountTest, CharactersOutsideTheBmp)
{
    // U+20BB7 (𠮷) is a Han character encoded as a surrogate pair.
    EXPECT_EQ(countWords(QString::fromUcs4(U"\U00020BB7\U00020BB7")), 2);
    // Emoji are symbols and are not counted.
    EXPECT_EQ(countWords(QString::fromUcs4(U"ok \U0001F600")), 1);
}
