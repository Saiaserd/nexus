#pragma once

#include <QPlainTextEdit>

#include "ui/Theme.h"

namespace nexus::ui {

// Markdown source editor (Main.dc.html): a centred column at most 820px wide with a 56px
// line-number gutter on its left, 15px text, and the current line tinted.
class NoteEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    static constexpr int kColumnMaxWidth = 820;
    static constexpr int kGutterWidth = 56;
    static constexpr int kGutterPadding = 20; // between the numbers and the text
    static constexpr int kRightPadding = 32;
    static constexpr int kTopPadding = 18;
    static constexpr int kLineNumberPixelSize = 12;

    explicit NoteEditor(const Palette& palette, QWidget* parent = nullptr);

    // Shows a note's document. Use instead of setDocument(): QPlainTextEdit only gives its own
    // initial document the widget font (15px from the style sheet).
    void showDocument(QTextDocument* document);
    void setPalette(const Palette& palette);
    void setLineNumbersVisible(bool visible);
    [[nodiscard]] bool lineNumbersVisible() const { return m_lineNumbers; }

    // 1-based line and column of the text cursor (column counts UTF-16 code units).
    [[nodiscard]] int cursorLine() const;
    [[nodiscard]] int cursorColumn() const;

signals:
    void focusLost();

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    class Gutter;

    void updateMargins();
    void updateCurrentLine();
    void updateGutter(const QRect& rect, int dy);
    void paintGutter(QPaintEvent* event);

    Palette m_palette;
    Gutter* m_gutter = nullptr;
    bool m_lineNumbers = true;
    int m_columnLeft = 0;
};

} // namespace nexus::ui
