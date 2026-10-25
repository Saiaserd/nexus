#pragma once

#include <memory>
#include <vector>

#include <QString>
#include <QWidget>

#include "core/NoteFiles.h"

class QLabel;
class QStackedWidget;
class QTabBar;
class QTimer;

namespace nexus::ui {

class MarkdownHighlighter;
class NoteEditor;
class NoteSession;
class ThemeManager;

// The editor area of the workspace (Main.dc.html main): tab bar, breadcrumb and one editor
// per open note. Unsaved tabs show a dot instead of the close button.
class EditorTabs : public QWidget {
    Q_OBJECT

public:
    explicit EditorTabs(ThemeManager& theme, QWidget* parent = nullptr);
    ~EditorTabs() override;

    EditorTabs(const EditorTabs&) = delete;
    EditorTabs& operator=(const EditorTabs&) = delete;
    EditorTabs(EditorTabs&&) = delete;
    EditorTabs& operator=(EditorTabs&&) = delete;

    // A vault was opened (closes every tab first). `files` null = no vault.
    void setVault(std::shared_ptr<const core::NoteFiles> files, const QString& vaultName);

    // Opens a vault-relative note. Already open: switches to it. `newTab` false replaces the
    // current tab (after saving it), like a single click in the file tree.
    void openNote(const QString& relPath, bool newTab = false);
    void saveCurrent();
    // Saves every tab and waits; false if a write failed.
    bool saveAllAndWait();
    void closeCurrentTab();
    void closeAll();

    // File tree changes.
    void renameNote(const QString& oldRel, const QString& newRel);
    void removeNote(const QString& relPath);

    void setLineNumbersVisible(bool visible);

    [[nodiscard]] int count() const;
    [[nodiscard]] NoteSession* currentSession() const;
    [[nodiscard]] NoteEditor* currentEditor() const;
    [[nodiscard]] NoteSession* sessionFor(const QString& relPath) const;
    [[nodiscard]] QString breadcrumbText() const;

signals:
    void currentChanged();
    // Unsaved state of a note changed (file tree dot).
    void dirtyChanged(const QString& relPath, bool dirty);
    // Save state, cursor or word count of the current tab changed.
    void statusChanged();
    void saveFailed(const QString& relPath, const QString& message);

private:
    struct Tab;
    class TabButton;

    void applyTheme();
    void addTab(const QString& relPath);
    void closeTab(int index);
    void onCurrentChanged(int index);
    void refreshTab(int index);
    void refreshBreadcrumb();
    [[nodiscard]] int indexOf(const QString& relPath) const;
    [[nodiscard]] int indexOf(const NoteSession* session) const;

    ThemeManager& m_theme;
    std::shared_ptr<const core::NoteFiles> m_files;
    QString m_vaultName;
    bool m_lineNumbers = true;

    QTabBar* m_tabBar = nullptr;
    QLabel* m_breadcrumb = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_hint = nullptr;
    std::vector<std::unique_ptr<Tab>> m_tabs; // same order as m_tabBar
};

} // namespace nexus::ui
