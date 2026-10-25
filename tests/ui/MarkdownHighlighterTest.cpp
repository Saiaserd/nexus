#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include "ui/Fonts.h"
#include "ui/MarkdownHighlighter.h"
#include "ui/Theme.h"

using nexus::parser::SpanKind;
using nexus::ui::lightPalette;
using nexus::ui::MarkdownHighlighter;

namespace {

// The format the highlighter put on the character at `pos` of block `line`.
QTextCharFormat formatAt(const QTextDocument& doc, int line, int pos)
{
    const QTextBlock block = doc.findBlockByNumber(line);
    for (const QTextLayout::FormatRange& range : block.layout()->formats()) {
        if (pos >= range.start && pos < range.start + range.length) {
            return range.format;
        }
    }
    return {};
}

// QSyntaxHighlighter runs its first pass from the event loop and ignores edits until then.
void settle()
{
    QCoreApplication::processEvents();
}

int pixelSize(const QTextCharFormat& format)
{
    return format.property(QTextFormat::FontPixelSize).toInt();
}

} // namespace

TEST(MarkdownHighlighterTest, Headings)
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("# Title\n## Sub\n### Third\n#### Four"));
    const MarkdownHighlighter highlighter(&doc, lightPalette());
    settle();
    const auto& p = lightPalette();

    EXPECT_EQ(formatAt(doc, 0, 0).foreground().color(), p.marker);
    EXPECT_EQ(pixelSize(formatAt(doc, 0, 0)), 24);
    EXPECT_EQ(formatAt(doc, 0, 2).fontWeight(), QFont::Bold);
    EXPECT_EQ(pixelSize(formatAt(doc, 0, 2)), 24);
    EXPECT_EQ(pixelSize(formatAt(doc, 1, 3)), 19);
    EXPECT_EQ(pixelSize(formatAt(doc, 2, 4)), 17);
    EXPECT_EQ(formatAt(doc, 3, 5).fontWeight(), QFont::Bold);
}

TEST(MarkdownHighlighterTest, WikiLinksAndTags)
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("see [[Note]] #tag"));
    const MarkdownHighlighter highlighter(&doc, lightPalette());
    settle();
    const auto& p = lightPalette();

    EXPECT_EQ(formatAt(doc, 0, 4).foreground().color(), p.marker); // [[
    const QTextCharFormat target = formatAt(doc, 0, 6);
    EXPECT_EQ(target.foreground().color(), p.link);
    EXPECT_TRUE(target.fontUnderline());
    EXPECT_EQ(formatAt(doc, 0, 10).foreground().color(), p.marker); // ]]
    const QTextCharFormat tag = formatAt(doc, 0, 13);
    EXPECT_EQ(tag.foreground().color(), p.tag);
    EXPECT_EQ(tag.background().color(), p.tagBg);
    // Plain text before the link has no format of its own.
    EXPECT_FALSE(formatAt(doc, 0, 0).hasProperty(QTextFormat::ForegroundBrush));
}

TEST(MarkdownHighlighterTest, TagInsideHeadingKeepsHeadingSize)
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("## Plan #m1"));
    const MarkdownHighlighter highlighter(&doc, lightPalette());
    settle();

    const QTextCharFormat tag = formatAt(doc, 0, 9);
    EXPECT_EQ(tag.foreground().color(), lightPalette().tag);
    EXPECT_EQ(tag.fontWeight(), QFont::Bold); // merged with the heading
}

TEST(MarkdownHighlighterTest, InlineCodeListAndEmphasis)
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("- a `code` **b** *c*"));
    const MarkdownHighlighter highlighter(&doc, lightPalette());
    settle();
    const auto& p = lightPalette();

    const QTextCharFormat bullet = formatAt(doc, 0, 0);
    EXPECT_EQ(bullet.foreground().color(), p.accent);
    EXPECT_EQ(bullet.fontWeight(), QFont::Bold);
    const QTextCharFormat code = formatAt(doc, 0, 5);
    EXPECT_EQ(code.background().color(), p.codeBg);
    EXPECT_EQ(code.fontFamilies().toStringList().value(0), nexus::ui::fonts::monoFamily());
    EXPECT_EQ(formatAt(doc, 0, 11).foreground().color(), p.marker); // **
    EXPECT_EQ(formatAt(doc, 0, 13).fontWeight(), QFont::Bold);      // b
    EXPECT_TRUE(formatAt(doc, 0, 18).fontItalic());                 // c
}

TEST(MarkdownHighlighterTest, QuoteIsItalicMuted)
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("> quoted"));
    const MarkdownHighlighter highlighter(&doc, lightPalette());
    settle();

    EXPECT_EQ(formatAt(doc, 0, 0).foreground().color(), lightPalette().marker);
    const QTextCharFormat text = formatAt(doc, 0, 3);
    EXPECT_TRUE(text.fontItalic());
    EXPECT_EQ(text.foreground().color(), lightPalette().muted);
}

TEST(MarkdownHighlighterTest, FenceStateCarriesAcrossBlocks)
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("```cpp\n# not a heading\n```\n# heading"));
    const MarkdownHighlighter highlighter(&doc, lightPalette());
    settle();
    const auto& p = lightPalette();

    EXPECT_EQ(formatAt(doc, 1, 0).background().color(), p.codeBg);
    EXPECT_NE(pixelSize(formatAt(doc, 1, 2)), 24);
    EXPECT_EQ(pixelSize(formatAt(doc, 3, 2)), 24);

    // Removing the closing fence turns the rest of the document into code. QTextDocument only
    // reports edits (and so re-highlights) once it has a layout, as it has in the editor.
    ASSERT_NE(doc.documentLayout(), nullptr);
    QTextCursor cursor(doc.findBlockByNumber(2));
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    EXPECT_EQ(formatAt(doc, 3, 2).background().color(), p.codeBg);
}

TEST(MarkdownHighlighterTest, PaletteSwitchRecolours)
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("#tag"));
    MarkdownHighlighter highlighter(&doc, lightPalette());
    settle();
    highlighter.setPalette(nexus::ui::darkPalette());
    EXPECT_EQ(formatAt(doc, 0, 1).foreground().color(), nexus::ui::darkPalette().tag);
}
