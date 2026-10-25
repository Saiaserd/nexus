#pragma once

#include <memory>
#include <vector>

#include <QAbstractItemModel>
#include <QHash>
#include <QStringList>

#include "core/Vault.h"

namespace nexus::ui {

// Tree of the folders and notes of a vault for the file tree. Folders come before notes and
// siblings use the core's natural order (core::pathLess). Paths are vault-relative, '/'
// separated. The model only mirrors the disk: it never touches files; the panel changes it
// after a file operation succeeded or the watcher reported a change.
class VaultTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum Role {
        RelPathRole = Qt::UserRole + 1, // QString, the vault-relative path
        IsFolderRole,                   // bool
        DirtyRole,                      // bool, a note with unsaved changes
        NoteCountRole,                  // int, notes below a folder (recursive)
    };

    explicit VaultTreeModel(QObject* parent = nullptr);
    ~VaultTreeModel() override;

    VaultTreeModel(const VaultTreeModel&) = delete;
    VaultTreeModel& operator=(const VaultTreeModel&) = delete;
    VaultTreeModel(VaultTreeModel&&) = delete;
    VaultTreeModel& operator=(VaultTreeModel&&) = delete;

    // Replaces everything with the snapshot.
    void setVault(const core::Vault& vault);

    // Incremental updates; all are idempotent. Missing parent folders are created.
    void addNote(const QString& rel);
    void addFolder(const QString& rel);
    // Removes a note or a folder with everything below it. Returns false if it is unknown.
    bool removePath(const QString& rel);
    // Moves a note or a folder (with its subtree, dirty flags included) to a new path.
    void renamePath(const QString& oldRel, const QString& newRel);
    void setDirty(const QString& rel, bool dirty);

    [[nodiscard]] bool contains(const QString& rel) const;
    [[nodiscard]] bool isFolder(const QString& rel) const;
    [[nodiscard]] QModelIndex indexForPath(const QString& rel) const;
    [[nodiscard]] QString relPath(const QModelIndex& index) const;
    [[nodiscard]] bool isFolder(const QModelIndex& index) const;
    // Notes at or below rel (just rel for a note), in tree order.
    [[nodiscard]] QStringList notesUnder(const QString& rel) const;
    // Folders strictly below rel ("" = the whole vault).
    [[nodiscard]] QStringList foldersUnder(const QString& rel) const;
    // True when a sibling of index (not index itself) already uses baseName (compared without
    // case, so it also holds on Windows and macOS).
    [[nodiscard]] bool siblingNameTaken(const QModelIndex& index, const QString& baseName) const;
    // Name shown for an item: the file name without ".md" for notes.
    [[nodiscard]] static QString displayName(const QString& rel, bool folder);
    [[nodiscard]] static QString parentPath(const QString& rel);

    // QAbstractItemModel
    [[nodiscard]] QModelIndex index(int row, int column,
                                    const QModelIndex& parent = {}) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    struct Node {
        QString rel;
        bool folder = false;
        bool dirty = false;
        Node* parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
    };

    [[nodiscard]] Node* find(const QString& rel) const;
    [[nodiscard]] Node* nodeFor(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexOf(const Node* node) const;
    [[nodiscard]] int rowOf(const Node* node) const;
    Node* ensureFolder(const QString& rel);
    Node* insertChild(Node* parent, const QString& rel, bool folder);
    void forget(const Node* node);
    static void collect(const Node* node, QStringList& notes, QStringList* folders);
    static int countNotes(const Node* node);

    std::unique_ptr<Node> m_root;
    QHash<QString, Node*> m_nodes;
    bool m_resetting = false;
};

} // namespace nexus::ui
