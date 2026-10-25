#pragma once

#include <QStringView>
#include <cstdint>
#include <vector>

namespace nexus::parser {

// What a span of a line is. "Marker" kinds are syntax characters (UI usually dims them);
// "Text"/"Target"/"Url" kinds are the content between them.
enum class SpanKind : std::uint8_t {
    HeadingMarker, // the leading '#'s
    HeadingText,   // rest of a heading line; Span::level is 1-6
    ListMarker,    // '-', '*', '+', '1.', '1)' and task markers '- [ ]' / '- [x]'
    QuoteMarker,   // each '>' of a block quote
    StrongMarker,  // '**' or '__'
    StrongText,
    EmphasisMarker, // '*' or '_'
    EmphasisText,
    InlineCode,      // whole `code` span including its backticks
    FenceDelimiter,  // opening / closing fence including the language label
    CodeBlockText,   // a whole line inside a fenced code block
    WikiLinkBracket, // '[[', ']]' and the '|' separating target and alias
    WikiLinkTarget,  // [[target]], [[target#heading]], [[target|alias]] -> "target#heading"
    WikiLinkAlias,   // the alias of [[target|alias]]
    Tag,             // '#tag' including the '#'
    LinkMarker,      // '[', '](' and ')' of [text](url)
    LinkText,
    LinkUrl,
    HorizontalRule,
};

struct Span {
    int start = 0;  // UTF-16 code unit offset in the line
    int length = 0; // UTF-16 code units
    SpanKind kind = SpanKind::HeadingMarker;
    int level = 0; // heading level for HeadingMarker/HeadingText, otherwise 0

    friend bool operator==(const Span&, const Span&) = default;
};

// State carried from one line to the next. Small value type; round-trips through an int so
// the UI can keep it in QTextBlock::userState (QSyntaxHighlighter::setCurrentBlockState).
// The default state ("not inside a code block") encodes as 0, which is also the value
// QSyntaxHighlighter reports for a block that was never highlighted (-1 is treated as default).
struct LexState {
    bool inFence = false;
    char16_t fenceChar = u'`'; // '`' or '~', only meaningful when inFence
    int fenceLength = 0;       // length of the opening fence, only meaningful when inFence

    [[nodiscard]] int toInt() const;
    [[nodiscard]] static LexState fromInt(int value);

    friend bool operator==(const LexState&, const LexState&) = default;
};

struct LineResult {
    // Sorted by start; when spans nest (e.g. a Tag inside HeadingText) the outer span comes
    // first, so applying them in order lets inner formats override outer ones.
    std::vector<Span> spans;
    LexState next;
};

// Lexes one line (no line terminator). Not a CommonMark parser: tuned for highlighting.
LineResult lexLine(QStringView line, LexState prev);

} // namespace nexus::parser
