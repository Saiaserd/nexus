#pragma once

#include <QWidget>

#include "core/FileError.h"

class QLabel;
class QSettings;
class QToolButton;
class QTreeView;

namespace nexus::ui {

class ElidedLabel;
class RenameDelegate;
class ThemeManager;
class VaultSession;
class VaultTreeModel;

// The left column of the workspace (Main.dc.html aside): a header with the vault name and the
// buttons "new note", "new folder", "collapse all", and the file tree below it. File
// operations run on QtConcurrent; the tree is updated on the UI thread once they succeeded.
class FileTreePanel : public QWidget {
    Q_OBJECT

public:
    explicit FileTreePanel(ThemeManager& theme, QWidget* parent = nullptr);

    // Follows the session: loads the tree when a vault opens, clears it on close, and applies
    // the watcher's changes incrementally.
    void setSession(VaultSession& session);
    // Where "don't ask again" for deleting is remembered; without it the dialog always shows.
    void setSettings(QSettings* settings) { m_settings = settings; }

    void setVaultName(const QString& name);
    [[nodiscard]] QString vaultName() const;

    // Shows or hides the unsaved dot of a note.
    void setNoteDirty(const QString& relPath, bool dirty);
    // Selects (and scrolls to) a note without asking to open it.
    void selectNote(const QString& relPath);

    // Creates a note in the folder of the current item (the root if none), then renames it.
    void newNote();
    void newFolder();

    [[nodiscard]] VaultTreeModel* model() const { return m_model; }
    [[nodiscard]] QTreeView* view() const { return m_view; }

signals:
    void openNoteRequested(const QString& relPath, bool newTab);
    void noteRenamed(const QString& oldRel, const QString& newRel);
    void noteRemoved(const QString& relPath);

private:
    void applyIcons();
    void onOpened();
    void onClosed();
    void onClicked(const QModelIndex& index);
    void activateCurrent(bool newTab);
    void showContextMenu(const QPoint& pos);
    void createNoteIn(const QString& folderRel);
    void createFolderIn(const QString& parentRel);
    void beginRename(const QString& relPath);
    void renameNow(const QString& relPath, const QString& newName);
    void deleteWithConfirmation(const QString& relPath);
    void deleteNow(const QString& relPath);
    void reconcileFolders();
    void expandTo(const QString& relPath);
    void revealInFileManager(const QString& relPath) const;
    void reportError(const QString& action, const core::FileError& error) const;
    [[nodiscard]] QString currentRel() const;
    [[nodiscard]] QString currentFolder() const;

    ThemeManager& m_theme;
    VaultSession* m_session = nullptr;
    QSettings* m_settings = nullptr;
    VaultTreeModel* m_model = nullptr;
    QTreeView* m_view = nullptr;
    RenameDelegate* m_delegate = nullptr;

    QLabel* m_icon = nullptr;
    ElidedLabel* m_name = nullptr;
    QToolButton* m_newNote = nullptr;
    QToolButton* m_newFolder = nullptr;
    QToolButton* m_collapse = nullptr;
    quint64 m_generation = 0; // bumped when the vault changes; stale task results are dropped
};

} // namespace nexus::ui
