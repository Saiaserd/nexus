#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include "parser/MarkdownLexer.h"
#include "ui/Theme.h"

namespace nexus::ui {

// Colours Markdown source with the spans from parser::lexLine (design: Main.dc.html). The
// lexer's fence state is kept in the block state (LexState::toInt), so editing one line only
// re-highlights the following lines while that state changes.
class MarkdownHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    MarkdownHighlighter(QTextDocument* document, const Palette& palette);

    // Rebuilds the formats for another theme and re-highlights the document.
    void setPalette(const Palette& palette);

    // The format applied to a span of this kind (level = heading level, else ignored).
    [[nodiscard]] QTextCharFormat formatFor(parser::SpanKind kind, int level = 0) const;
    // Applied after the last '>' of a block quote line.
    [[nodiscard]] const QTextCharFormat& quoteFormat() const { return m_quote; }

    // Pixel size of the heading text for a level (1-6); body text is kBodyPixelSize.
    static int headingPixelSize(int level);
    static constexpr int kBodyPixelSize = 15;
    static constexpr int kCodePixelSize = 13;

protected:
    void highlightBlock(const QString& text) override;

private:
    void buildFormats(const Palette& palette);
    // Merges `format` into the range, keeping what outer spans already set.
    void mergeFormat(int start, int length, const QTextCharFormat& format);

    Palette m_palette;
    QTextCharFormat m_quote;
};

} // namespace nexus::ui
