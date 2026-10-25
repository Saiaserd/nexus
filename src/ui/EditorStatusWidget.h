#pragma once

#include <QTimer>
#include <QWidget>

class QLabel;

namespace nexus::ui {

class EditorTabs;

// Right-hand part of the status bar (Main.dc.html footer): save state, cursor position, word
// count, encoding and line ending of the current tab. Hidden while no tab is open.
class EditorStatusWidget : public QWidget {
    Q_OBJECT

public:
    explicit EditorStatusWidget(EditorTabs& tabs, QWidget* parent = nullptr);

    // "已儲存 · 第 9 行，第 24 欄 · 186 字 · UTF-8 · LF" (for tests and screenshots).
    [[nodiscard]] QString summary() const;

private:
    void refresh();
    void refreshWordCount();

    EditorTabs& m_tabs;
    QLabel* m_save = nullptr;
    QLabel* m_cursor = nullptr;
    QLabel* m_words = nullptr;
    QLabel* m_encoding = nullptr;
    QTimer m_wordTimer; // counting a long note on every key press would be wasteful
};

} // namespace nexus::ui
