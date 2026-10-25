#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "TestSupport.h"
#include "core/NoteFiles.h"

using nexus::core::FileErrorKind;
using nexus::core::LineEnding;
using nexus::core::NameError;
using nexus::core::NoteDocument;
using nexus::core::NoteFiles;
using nexus::core::NoteInfo;
using nexus::core::validateName;
using nexus::test::exists;
using nexus::test::makeDir;
using nexus::test::readFile;
using nexus::test::writeFile;

namespace {

QByteArray withBom(const QByteArray& body)
{
    return QByteArray::fromHex("EFBBBF") + body;
}

// gtest fixture idiom: tests read the members directly.
// NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
class NoteFilesTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        m_root = m_dir.path();
    }

    QTemporaryDir m_dir;
    QString m_root;
};
// NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

std::optional<NameError> check(const char* utf8)
{
    return validateName(QString::fromUtf8(utf8));
}

} // namespace

// ---- validateName -----------------------------------------------------------------------

TEST(ValidateNameTest, AcceptsOrdinaryNames)
{
    EXPECT_EQ(check("note"), std::nullopt);
    EXPECT_EQ(check("我的 筆記 1"), std::nullopt);
    EXPECT_EQ(check("a.b"), std::nullopt);
    EXPECT_EQ(check("console"), std::nullopt);
    EXPECT_EQ(check("COM10"), std::nullopt);
    EXPECT_EQ(check("COM0"), std::nullopt);
    EXPECT_EQ(check("emoji 😀"), std::nullopt);
}

TEST(ValidateNameTest, RejectsEmpty)
{
    EXPECT_EQ(check(""), NameError::Empty);
}

TEST(ValidateNameTest, RejectsForbiddenCharacters)
{
    for (const char* name : {"a\\b", "a/b", "a:b", "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b"}) {
        EXPECT_EQ(check(name), NameError::InvalidCharacter) << name;
    }
}

TEST(ValidateNameTest, RejectsControlCharacters)
{
    EXPECT_EQ(validateName(QStringLiteral("a\tb")), NameError::ControlCharacter);
    EXPECT_EQ(validateName(QString(QChar(0x01))), NameError::ControlCharacter);
    EXPECT_EQ(validateName(QString(QChar(0x7F))), NameError::ControlCharacter);
    EXPECT_EQ(validateName(QStringLiteral("a\nb")), NameError::ControlCharacter);
}

TEST(ValidateNameTest, RejectsTrailingSpaceOrDot)
{
    EXPECT_EQ(check("name "), NameError::TrailingSpaceOrDot);
    EXPECT_EQ(check("name."), NameError::TrailingSpaceOrDot);
    EXPECT_EQ(check("."), NameError::TrailingSpaceOrDot);
    EXPECT_EQ(check(".."), NameError::TrailingSpaceOrDot);
    EXPECT_EQ(check("   "), NameError::TrailingSpaceOrDot);
}

TEST(ValidateNameTest, RejectsReservedWindowsNames)
{
    for (const char* name : {"CON", "con", "Prn", "AUX", "nul", "COM1", "com9", "LPT1", "lpt9",
                             "CON.txt", "nul.tar.gz", "Com3.md", "CON .txt"}) {
        EXPECT_EQ(check(name), NameError::ReservedName) << name;
    }
}

TEST(ValidateNameTest, RejectsTooLongNames)
{
    EXPECT_EQ(validateName(QString(250, u'a')), std::nullopt);
    EXPECT_EQ(validateName(QString(251, u'a')), NameError::TooLong);
    // 84 CJK characters are 252 UTF-8 bytes.
    EXPECT_EQ(validateName(QString(84, QChar(0x4E2D))), NameError::TooLong);
}

// ---- read / write -----------------------------------------------------------------------

TEST_F(NoteFilesTest, RoundTripKeepsBytesForEveryFormat)
{
    const std::vector<QByteArray> samples = {
        "",
        "plain\n",
        "no trailing newline",
        "line1\r\nline2\r\n",
        withBom("bom lf\n"),
        withBom("bom crlf\r\nsecond\r\n"),
        "\xE4\xB8\xAD\xE6\x96\x87 \xF0\x9F\x98\x80\n",
        "\n\n\n",
        withBom(""),
        "lone cr\rstays\n",
    };
    const NoteFiles files(m_root);
    int index = 0;
    for (const QByteArray& original : samples) {
        const QString rel = QString("n%1.md").arg(index++);
        ASSERT_TRUE(writeFile(m_root, rel, original));
        const auto doc = files.read(rel);
        ASSERT_TRUE(doc.has_value()) << rel.toStdString();
        EXPECT_FALSE(doc->text.contains(QStringLiteral("\r\n")));
        ASSERT_TRUE(files.write(rel, *doc).has_value());
        EXPECT_EQ(readFile(m_root, rel), original) << rel.toStdString();
    }
}

TEST_F(NoteFilesTest, ReadReportsLineEndingAndBom)
{
    ASSERT_TRUE(writeFile(m_root, "crlf.md", withBom("a\r\nb\r\n")));
    ASSERT_TRUE(writeFile(m_root, "lf.md", "a\nb\n"));
    const NoteFiles files(m_root);

    const auto crlf = files.read("crlf.md");
    ASSERT_TRUE(crlf.has_value());
    EXPECT_EQ(crlf->text, "a\nb\n");
    EXPECT_EQ(crlf->lineEnding, LineEnding::Crlf);
    EXPECT_TRUE(crlf->hasBom);

    const auto lf = files.read("lf.md");
    ASSERT_TRUE(lf.has_value());
    EXPECT_EQ(lf->lineEnding, LineEnding::Lf);
    EXPECT_FALSE(lf->hasBom);
}

TEST_F(NoteFilesTest, ReadRejectsInvalidUtf8WithoutTouchingTheFile)
{
    const QByteArray bad("ok \xFF\xFE broken\n");
    ASSERT_TRUE(writeFile(m_root, "bad.md", bad));
    const NoteFiles files(m_root);
    const auto doc = files.read("bad.md");
    ASSERT_FALSE(doc.has_value());
    EXPECT_EQ(doc.error().kind, FileErrorKind::InvalidEncoding);
    EXPECT_EQ(doc.error().path, "bad.md");
    EXPECT_EQ(readFile(m_root, "bad.md"), bad);

    // Truncated multi-byte sequence.
    ASSERT_TRUE(writeFile(m_root, "cut.md", "abc\xE4\xB8"));
    EXPECT_EQ(files.read("cut.md").error().kind, FileErrorKind::InvalidEncoding);
}

TEST_F(NoteFilesTest, ReadErrors)
{
    ASSERT_TRUE(makeDir(m_root, "folder"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.read("missing.md").error().kind, FileErrorKind::NotFound);
    EXPECT_EQ(files.read("folder").error().kind, FileErrorKind::NotAFile);
    EXPECT_EQ(files.read("../x.md").error().kind, FileErrorKind::InvalidPath);
    EXPECT_EQ(files.read("").error().kind, FileErrorKind::InvalidPath);
}

TEST_F(NoteFilesTest, WriteCreatesAndOverwrites)
{
    ASSERT_TRUE(makeDir(m_root, "dir"));
    const NoteFiles files(m_root);
    NoteDocument doc;
    doc.text = QStringLiteral("第一行\n第二行");
    ASSERT_TRUE(files.write("dir/new.md", doc).has_value());
    EXPECT_EQ(readFile(m_root, "dir/new.md"), doc.text.toUtf8());

    doc.text = QStringLiteral("short");
    doc.lineEnding = LineEnding::Crlf;
    doc.hasBom = true;
    ASSERT_TRUE(files.write("dir/new.md", doc).has_value());
    EXPECT_EQ(readFile(m_root, "dir/new.md"), withBom("short"));
}

TEST_F(NoteFilesTest, WriteErrors)
{
    ASSERT_TRUE(makeDir(m_root, "folder"));
    const NoteFiles files(m_root);
    const NoteDocument doc{QStringLiteral("x"), LineEnding::Lf, false};
    EXPECT_EQ(files.write("nodir/a.md", doc).error().kind, FileErrorKind::NotFound);
    EXPECT_EQ(files.write("folder", doc).error().kind, FileErrorKind::NotAFile);
    EXPECT_EQ(files.write("../escape.md", doc).error().kind, FileErrorKind::InvalidPath);
    EXPECT_FALSE(exists(m_root, "nodir"));
}

TEST_F(NoteFilesTest, WriteLeavesNoTemporaryFiles)
{
    const NoteFiles files(m_root);
    const NoteDocument doc{QStringLiteral("x"), LineEnding::Lf, false};
    ASSERT_TRUE(files.write("a.md", doc).has_value());
    EXPECT_EQ(QDir(m_root).entryList(QDir::Files | QDir::Hidden), QStringList{"a.md"});
}

TEST_F(NoteFilesTest, WriteListenerReceivesNewStamp)
{
    NoteFiles files(m_root);
    std::vector<NoteInfo> seen;
    files.setWriteListener([&seen](const NoteInfo& info) { seen.push_back(info); });
    const NoteDocument doc{QStringLiteral("12345"), LineEnding::Lf, false};
    ASSERT_TRUE(files.write("a.md", doc).has_value());
    ASSERT_EQ(seen.size(), 1U);
    EXPECT_EQ(seen[0].relPath, "a.md");
    EXPECT_EQ(seen[0].size, 5);
    EXPECT_EQ(seen[0].modified, QFileInfo(m_root + "/a.md").lastModified());

    // A failed write must not notify.
    EXPECT_FALSE(files.write("nodir/a.md", doc).has_value());
    EXPECT_EQ(seen.size(), 1U);
}

// ---- create -----------------------------------------------------------------------------

TEST_F(NoteFilesTest, CreateNoteAddsNumberOnConflict)
{
    ASSERT_TRUE(makeDir(m_root, "dir"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.createNote("").value(), "未命名.md");
    EXPECT_EQ(files.createNote("").value(), "未命名 1.md");
    EXPECT_EQ(files.createNote("").value(), "未命名 2.md");
    EXPECT_EQ(files.createNote("dir", "想法").value(), "dir/想法.md");
    EXPECT_EQ(files.createNote("dir", "想法").value(), "dir/想法 1.md");
    EXPECT_TRUE(exists(m_root, "未命名 2.md"));
    EXPECT_EQ(readFile(m_root, "dir/想法.md"), QByteArray());
}

TEST_F(NoteFilesTest, CreateNoteErrors)
{
    ASSERT_TRUE(writeFile(m_root, "file.md"));
    const NoteFiles files(m_root);
    const auto bad = files.createNote("", "a/b");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().kind, FileErrorKind::InvalidName);
    EXPECT_EQ(bad.error().nameError, NameError::InvalidCharacter);
    EXPECT_EQ(files.createNote("missing").error().kind, FileErrorKind::NotFound);
    EXPECT_EQ(files.createNote("file.md").error().kind, FileErrorKind::NotADirectory);
    EXPECT_EQ(files.createNote("../up").error().kind, FileErrorKind::InvalidPath);
    EXPECT_EQ(files.createNote("", "CON").error().nameError, NameError::ReservedName);
}

TEST_F(NoteFilesTest, CreateFolder)
{
    ASSERT_TRUE(writeFile(m_root, "file.md"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.createFolder("", "專案").value(), "專案");
    EXPECT_EQ(files.createFolder("專案", "子").value(), "專案/子");
    EXPECT_TRUE(QFileInfo(m_root + "/專案/子").isDir());

    EXPECT_EQ(files.createFolder("", "專案").error().kind, FileErrorKind::AlreadyExists);
    EXPECT_EQ(files.createFolder("", "file.md").error().kind, FileErrorKind::AlreadyExists);
    EXPECT_EQ(files.createFolder("", "bad?").error().kind, FileErrorKind::InvalidName);
    EXPECT_EQ(files.createFolder("nope", "x").error().kind, FileErrorKind::NotFound);
    EXPECT_EQ(files.createFolder("file.md", "x").error().kind, FileErrorKind::NotADirectory);
}

// ---- rename -----------------------------------------------------------------------------

TEST_F(NoteFilesTest, RenameNoteKeepsExtension)
{
    ASSERT_TRUE(writeFile(m_root, "dir/old.md", "body"));
    ASSERT_TRUE(writeFile(m_root, "UPPER.MD"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.rename("dir/old.md", "新名稱").value(), "dir/新名稱.md");
    EXPECT_FALSE(exists(m_root, "dir/old.md"));
    EXPECT_EQ(readFile(m_root, "dir/新名稱.md"), QByteArray("body"));
    EXPECT_EQ(files.rename("UPPER.MD", "lower").value(), "lower.MD");
}

TEST_F(NoteFilesTest, RenameFolderMovesContents)
{
    ASSERT_TRUE(writeFile(m_root, "a/b/note.md", "x"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.rename("a", "renamed.v2").value(), "renamed.v2");
    EXPECT_TRUE(exists(m_root, "renamed.v2/b/note.md"));
    EXPECT_EQ(files.rename("renamed.v2/b", "c").value(), "renamed.v2/c");
    EXPECT_TRUE(exists(m_root, "renamed.v2/c/note.md"));
}

TEST_F(NoteFilesTest, RenameToSameNameIsANoOp)
{
    ASSERT_TRUE(writeFile(m_root, "same.md", "x"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.rename("same.md", "same").value(), "same.md");
    EXPECT_EQ(readFile(m_root, "same.md"), QByteArray("x"));
}

TEST_F(NoteFilesTest, RenameOnlyChangingCaseWorks)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "content"));
    ASSERT_TRUE(makeDir(m_root, "folder"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.rename("a.md", "A").value(), "A.md");
    EXPECT_EQ(files.rename("folder", "Folder").value(), "Folder");
    EXPECT_EQ(readFile(m_root, "A.md"), QByteArray("content"));
    const QStringList names = QDir(m_root).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    EXPECT_TRUE(names.contains("A.md"));
    EXPECT_TRUE(names.contains("Folder"));
    EXPECT_FALSE(names.contains("a.md"));
    EXPECT_FALSE(names.contains("folder"));
    EXPECT_EQ(names.size(), 2); // no leftover temporary names
}

TEST_F(NoteFilesTest, RenameConflicts)
{
    ASSERT_TRUE(writeFile(m_root, "a.md", "A"));
    ASSERT_TRUE(writeFile(m_root, "b.md", "B"));
    ASSERT_TRUE(makeDir(m_root, "dir"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.rename("a.md", "b").error().kind, FileErrorKind::AlreadyExists);
    EXPECT_EQ(files.rename("dir", "b.md").error().kind, FileErrorKind::AlreadyExists);
    EXPECT_EQ(readFile(m_root, "a.md"), QByteArray("A"));
    EXPECT_EQ(readFile(m_root, "b.md"), QByteArray("B"));
    // Name clashes that differ only by case are conflicts on Windows / macOS only.
    const auto result = files.rename("b.md", "A");
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, FileErrorKind::AlreadyExists);
    EXPECT_EQ(readFile(m_root, "a.md"), QByteArray("A"));
#else
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "A.md");
#endif
}

TEST_F(NoteFilesTest, RenameErrors)
{
    ASSERT_TRUE(writeFile(m_root, "a.md"));
    const NoteFiles files(m_root);
    EXPECT_EQ(files.rename("missing.md", "x").error().kind, FileErrorKind::NotFound);
    EXPECT_EQ(files.rename("../a.md", "x").error().kind, FileErrorKind::InvalidPath);
    const auto bad = files.rename("a.md", "x|y");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().kind, FileErrorKind::InvalidName);
    EXPECT_EQ(bad.error().nameError, NameError::InvalidCharacter);
    EXPECT_EQ(files.rename("a.md", "").error().nameError, NameError::Empty);
    EXPECT_EQ(files.rename("a.md", "nul").error().nameError, NameError::ReservedName);
    EXPECT_TRUE(exists(m_root, "a.md"));
}

// ---- trash ------------------------------------------------------------------------------

TEST_F(NoteFilesTest, MoveToTrashRemovesNotesAndFolders)
{
    ASSERT_TRUE(writeFile(m_root, "gone.md", "x"));
    ASSERT_TRUE(writeFile(m_root, "gonedir/inner.md", "x"));
    const NoteFiles files(m_root);
    const auto note = files.moveToTrash("gone.md");
    if (!note.has_value() && note.error().kind == FileErrorKind::TrashFailed) {
        GTEST_SKIP() << "no trash available in this environment";
    }
    ASSERT_TRUE(note.has_value());
    EXPECT_FALSE(exists(m_root, "gone.md"));
    ASSERT_TRUE(files.moveToTrash("gonedir").has_value());
    EXPECT_FALSE(exists(m_root, "gonedir"));
}

TEST_F(NoteFilesTest, MoveToTrashErrors)
{
    const NoteFiles files(m_root);
    EXPECT_EQ(files.moveToTrash("missing.md").error().kind, FileErrorKind::NotFound);
    EXPECT_EQ(files.moveToTrash("../x").error().kind, FileErrorKind::InvalidPath);
}
