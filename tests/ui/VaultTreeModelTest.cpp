#include <gtest/gtest.h>

#include <QAbstractItemModelTester>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "TestSupport.h"
#include "core/Vault.h"
#include "ui/VaultTreeModel.h"

using nexus::test::writeFile;
using nexus::ui::VaultTreeModel;

namespace {

class VaultTreeModelTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_TRUE(m_dir.isValid());
        for (const char* rel : {"README.md", "specs/vault-管理.md", "specs/note10.md",
                                "specs/note2.md", "decisions/0001.md", "inbox/a/deep.md"}) {
            ASSERT_TRUE(writeFile(m_dir.filePath(QString::fromUtf8(rel))));
        }
        ASSERT_TRUE(QDir(m_dir.path()).mkpath(QStringLiteral("empty")));
        reload();
    }

    void reload()
    {
        auto vault = nexus::core::Vault::open(m_dir.path());
        ASSERT_TRUE(vault.has_value());
        m_model.setVault(*vault);
    }

    // The display names of the children of a folder ("" = root), in row order.
    [[nodiscard]] QStringList childrenOf(const QString& rel) const
    {
        const QModelIndex parent = m_model.indexForPath(rel);
        QStringList names;
        for (int row = 0; row < m_model.rowCount(parent); ++row) {
            names << m_model.index(row, 0, parent).data().toString();
        }
        return names;
    }

    QTemporaryDir m_dir;
    VaultTreeModel m_model;
    QAbstractItemModelTester m_tester{&m_model,
                                      QAbstractItemModelTester::FailureReportingMode::Fatal};
};

} // namespace

TEST_F(VaultTreeModelTest, FoldersComeFirstThenNotesInNaturalOrderWithoutExtension)
{
    EXPECT_EQ(childrenOf(QString()),
              (QStringList{"decisions", "empty", "inbox", "specs", "README"}));
    EXPECT_EQ(childrenOf(QStringLiteral("specs")),
              (QStringList{"note2", "note10", QStringLiteral("vault-管理")}));
}

TEST_F(VaultTreeModelTest, ExposesPathsAndKinds)
{
    const QModelIndex note = m_model.indexForPath(QStringLiteral("specs/note2.md"));
    ASSERT_TRUE(note.isValid());
    EXPECT_EQ(note.data(VaultTreeModel::RelPathRole).toString(), QStringLiteral("specs/note2.md"));
    EXPECT_FALSE(note.data(VaultTreeModel::IsFolderRole).toBool());
    EXPECT_EQ(m_model.parent(note), m_model.indexForPath(QStringLiteral("specs")));
    EXPECT_TRUE(m_model.isFolder(QStringLiteral("empty")));
    const auto noteCount = [this](const QString& rel) {
        return m_model.indexForPath(rel).data(VaultTreeModel::NoteCountRole).toInt();
    };
    EXPECT_EQ(noteCount(QStringLiteral("specs")), 3);
    EXPECT_EQ(noteCount(QStringLiteral("inbox")), 1);
    EXPECT_EQ(noteCount(QStringLiteral("empty")), 0);
}

TEST_F(VaultTreeModelTest, AddNoteKeepsOrderCreatesParentsAndIsIdempotent)
{
    m_model.addNote(QStringLiteral("specs/note3.md"));
    m_model.addNote(QStringLiteral("specs/note3.md"));
    EXPECT_EQ(childrenOf(QStringLiteral("specs")),
              (QStringList{"note2", "note3", "note10", QStringLiteral("vault-管理")}));

    m_model.addNote(QStringLiteral("new/sub/x.md"));
    EXPECT_TRUE(m_model.isFolder(QStringLiteral("new")));
    EXPECT_TRUE(m_model.isFolder(QStringLiteral("new/sub")));
    EXPECT_EQ(childrenOf(QStringLiteral("new/sub")), (QStringList{"x"}));
    EXPECT_EQ(childrenOf(QString()),
              (QStringList{"decisions", "empty", "inbox", "new", "specs", "README"}));
}

TEST_F(VaultTreeModelTest, RemovePathRemovesSubtreeAndReportsUnknown)
{
    EXPECT_TRUE(m_model.removePath(QStringLiteral("inbox")));
    EXPECT_FALSE(m_model.contains(QStringLiteral("inbox/a/deep.md")));
    EXPECT_FALSE(m_model.removePath(QStringLiteral("inbox")));
    EXPECT_TRUE(m_model.removePath(QStringLiteral("README.md")));
    EXPECT_EQ(childrenOf(QString()), (QStringList{"decisions", "empty", "specs"}));
}

TEST_F(VaultTreeModelTest, RenameNoteReordersAndKeepsDirtyFlag)
{
    m_model.setDirty(QStringLiteral("specs/note2.md"), true);
    m_model.renamePath(QStringLiteral("specs/note2.md"), QStringLiteral("specs/zzz.md"));
    EXPECT_EQ(childrenOf(QStringLiteral("specs")),
              (QStringList{"note10", QStringLiteral("vault-管理"), "zzz"}));
    EXPECT_TRUE(m_model.indexForPath(QStringLiteral("specs/zzz.md"))
                    .data(VaultTreeModel::DirtyRole)
                    .toBool());
    EXPECT_FALSE(m_model.contains(QStringLiteral("specs/note2.md")));
}

TEST_F(VaultTreeModelTest, RenameFolderMovesTheWholeSubtreeIncludingEmptyFolders)
{
    ASSERT_TRUE(QDir(m_dir.path()).mkpath(QStringLiteral("inbox/emptysub")));
    reload();
    m_model.setDirty(QStringLiteral("inbox/a/deep.md"), true);

    m_model.renamePath(QStringLiteral("inbox"), QStringLiteral("zeta"));
    EXPECT_FALSE(m_model.contains(QStringLiteral("inbox")));
    EXPECT_TRUE(m_model.contains(QStringLiteral("zeta/a/deep.md")));
    EXPECT_TRUE(m_model.isFolder(QStringLiteral("zeta/emptysub")));
    EXPECT_TRUE(m_model.indexForPath(QStringLiteral("zeta/a/deep.md"))
                    .data(VaultTreeModel::DirtyRole)
                    .toBool());
    EXPECT_EQ(childrenOf(QString()),
              (QStringList{"decisions", "empty", "specs", "zeta", "README"}));
}

TEST_F(VaultTreeModelTest, SetDirtyEmitsDataChangedOnlyOnChange)
{
    const QModelIndex idx = m_model.indexForPath(QStringLiteral("README.md"));
    QSignalSpy spy(&m_model, &QAbstractItemModel::dataChanged);
    m_model.setDirty(QStringLiteral("README.md"), true);
    m_model.setDirty(QStringLiteral("README.md"), true);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(idx.data(VaultTreeModel::DirtyRole).toBool());
    m_model.setDirty(QStringLiteral("nope.md"), true); // unknown paths are ignored
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(VaultTreeModelTest, SiblingNameTakenIgnoresCaseAndItself)
{
    const QModelIndex note = m_model.indexForPath(QStringLiteral("specs/note2.md"));
    EXPECT_TRUE(m_model.siblingNameTaken(note, QStringLiteral("NOTE10")));
    EXPECT_FALSE(m_model.siblingNameTaken(note, QStringLiteral("note2"))); // itself
    EXPECT_FALSE(m_model.siblingNameTaken(note, QStringLiteral("free")));
    const QModelIndex folder = m_model.indexForPath(QStringLiteral("empty"));
    EXPECT_TRUE(m_model.siblingNameTaken(folder, QStringLiteral("specs")));
    // A note called "specs" would be specs.md: no clash with the folder.
    EXPECT_FALSE(m_model.siblingNameTaken(m_model.indexForPath(QStringLiteral("README.md")),
                                          QStringLiteral("specs")));
}

TEST_F(VaultTreeModelTest, NotesAndFoldersUnder)
{
    EXPECT_EQ(m_model.notesUnder(QStringLiteral("inbox")), (QStringList{"inbox/a/deep.md"}));
    EXPECT_EQ(m_model.notesUnder(QStringLiteral("README.md")), (QStringList{"README.md"}));
    EXPECT_EQ(m_model.foldersUnder(QStringLiteral("inbox")), (QStringList{"inbox/a"}));
    EXPECT_TRUE(m_model.notesUnder(QStringLiteral("missing")).isEmpty());
}

TEST_F(VaultTreeModelTest, SetVaultReplacesEverything)
{
    m_model.setVault(nexus::core::Vault());
    EXPECT_EQ(m_model.rowCount(), 0);
    EXPECT_FALSE(m_model.contains(QStringLiteral("README.md")));
}
