#pragma once

#include <QFrame>

class QSettings;

namespace nexus::ui {

class FileTreePanel;
class ThemeManager;
class VaultSession;

// Left column of the workspace: the file tree with its header (vault name and buttons). It
// forwards the panel's signals so the main window does not have to know about the panel.
class VaultSidebar : public QFrame {
    Q_OBJECT

public:
    explicit VaultSidebar(ThemeManager& theme, QSettings* settings = nullptr,
                          QWidget* parent = nullptr);

    // The tree follows this session (opened, closed, watcher changes).
    void setSession(VaultSession& session);

    void setVault(const QString& name, int noteCount);
    // The note count is shown in the status bar; the sidebar has no use for it.
    void setNoteCount(int count);
    [[nodiscard]] QString vaultName() const;

    // Unsaved dot next to a note.
    void setNoteDirty(const QString& relPath, bool dirty);
    // Highlights a note (the one shown in the editor) without asking to open it.
    void selectNote(const QString& relPath);
    // Creates a note in the current folder and lets the user name it.
    void newNote();

    [[nodiscard]] FileTreePanel* panel() const { return m_panel; }

signals:
    void openNoteRequested(const QString& relPath, bool newTab);
    void noteRenamed(const QString& oldRel, const QString& newRel);
    void noteRemoved(const QString& relPath);

private:
    FileTreePanel* m_panel = nullptr;
};

} // namespace nexus::ui
