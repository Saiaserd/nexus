#include "ui/MarkdownHighlighter.h"

#include <QFont>
#include <QTextDocument>

#include "ui/Fonts.h"

namespace nexus::ui {

using parser::SpanKind;

namespace {

void setPixelSize(QTextCharFormat& format, int pixelSize)
{
    format.setProperty(QTextFormat::FontPixelSize, pixelSize);
}

void setMono(QTextCharFormat& format, const QColor& background)
{
    format.setFontFamilies({fonts::monoFamily()});
    format.setFontFixedPitch(true);
    setPixelSize(format, MarkdownHighlighter::kCodePixelSize);
    format.setBackground(background);
}

} // namespace

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* document, const Palette& palette)
    : QSyntaxHighlighter(document)
{
    // QSyntaxHighlighter highlights the whole document once the event loop runs; doing it here
    // as well would double the cost of opening a long note.
    buildFormats(palette);
}

void MarkdownHighlighter::setPalette(const Palette& palette)
{
    buildFormats(palette);
    rehighlight();
}

void MarkdownHighlighter::buildFormats(const Palette& palette)
{
    m_palette = palette;
    m_quote = QTextCharFormat();
    m_quote.setFontItalic(true);
    m_quote.setForeground(m_palette.muted);
}

int MarkdownHighlighter::headingPixelSize(int level)
{
    switch (level) {
    case 1:
        return 24;
    case 2:
        return 19;
    case 3:
        return 17;
    default:
        return kBodyPixelSize;
    }
}

QTextCharFormat MarkdownHighlighter::formatFor(SpanKind kind, int level) const
{
    const Palette& p = m_palette;
    QTextCharFormat format;
    switch (kind) {
    case SpanKind::HeadingMarker:
        format.setForeground(p.marker);
        format.setFontWeight(QFont::Medium);
        setPixelSize(format, headingPixelSize(level));
        break;
    case SpanKind::HeadingText:
        format.setFontWeight(QFont::Bold);
        setPixelSize(format, headingPixelSize(level));
        break;
    case SpanKind::ListMarker:
        format.setForeground(p.accent);
        format.setFontWeight(QFont::Bold);
        break;
    case SpanKind::QuoteMarker:
    case SpanKind::StrongMarker:
    case SpanKind::EmphasisMarker:
    case SpanKind::WikiLinkBracket:
    case SpanKind::LinkMarker:
    case SpanKind::HorizontalRule:
        format.setForeground(p.marker);
        break;
    case SpanKind::StrongText:
        format.setFontWeight(QFont::Bold);
        break;
    case SpanKind::EmphasisText:
        format.setFontItalic(true);
        break;
    case SpanKind::InlineCode:
    case SpanKind::CodeBlockText:
        setMono(format, p.codeBg);
        break;
    case SpanKind::FenceDelimiter:
        setMono(format, p.codeBg);
        format.setForeground(p.marker);
        break;
    case SpanKind::WikiLinkTarget:
    case SpanKind::WikiLinkAlias:
    case SpanKind::LinkText:
        format.setForeground(p.link);
        format.setFontUnderline(true);
        break;
    case SpanKind::Tag:
        format.setForeground(p.tag);
        format.setBackground(p.tagBg);
        setPixelSize(format, kCodePixelSize);
        break;
    case SpanKind::LinkUrl:
        format.setForeground(p.muted);
        break;
    }
    return format;
}

void MarkdownHighlighter::mergeFormat(int start, int length, const QTextCharFormat& format)
{
    // setFormat() replaces; nested spans (a tag inside a heading) must keep the outer size.
    const int end = start + length;
    int runStart = start;
    while (runStart < end) {
        const QTextCharFormat current = this->format(runStart);
        int runEnd = runStart + 1;
        while (runEnd < end && this->format(runEnd) == current) {
            ++runEnd;
        }
        QTextCharFormat merged = current;
        merged.merge(format);
        setFormat(runStart, runEnd - runStart, merged);
        runStart = runEnd;
    }
}

void MarkdownHighlighter::highlightBlock(const QString& text)
{
    const int previous = previousBlockState();
    const parser::LineResult result =
        parser::lexLine(text, parser::LexState::fromInt(previous < 0 ? 0 : previous));

    int quoteEnd = -1;
    for (const parser::Span& span : result.spans) {
        if (span.kind == SpanKind::QuoteMarker) {
            quoteEnd = span.start + span.length;
        }
    }
    if (quoteEnd >= 0 && quoteEnd < text.size()) {
        setFormat(quoteEnd, static_cast<int>(text.size()) - quoteEnd, m_quote);
    }

    for (const parser::Span& span : result.spans) {
        mergeFormat(span.start, span.length, formatFor(span.kind, span.level));
    }
    setCurrentBlockState(result.next.toInt());
}

} // namespace nexus::ui
