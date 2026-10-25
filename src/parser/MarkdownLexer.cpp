#include "parser/MarkdownLexer.h"

#include <QChar>
#include <algorithm>

namespace nexus::parser {

namespace {

using Idx = qsizetype;

constexpr int kMaxFenceLength = 1 << 24;
constexpr int kMaxInlineDepth = 8;
constexpr Idx kMaxIndentForBlocks = 3;

// Worst-case guards so one hostile line (minified JSON, base64, a wall of '*') cannot stall the
// UI thread. Every "look ahead for a closing delimiter" search is bounded twice:
//  - kMaxSearchDistance: a single search looks at most this many UTF-16 units ahead; a closer
//    farther away is treated as missing and the opener stays plain text.
//  - kSearchBudgetPerUnit * line length + kSearchBudgetBase: total units all searches of one
//    line may scan together. Once spent, further searches fail immediately (plain text).
// Together they make lexing a line O(n); ordinary lines never get near either limit.
constexpr Idx kMaxSearchDistance = 4096;
constexpr Idx kSearchBudgetPerUnit = 8;
constexpr Idx kSearchBudgetBase = 4096;

struct Ctx {
    QStringView line;
    std::vector<Span>* out = nullptr;
    Idx budget = 0; // remaining look-ahead budget for this line
};

// End (exclusive) of the window a search starting at `start` may scan.
Idx searchLimit(const Ctx& c, Idx start, Idx to)
{
    return start + std::min({to - start, kMaxSearchDistance, std::max<Idx>(c.budget, 0)});
}

void chargeSearch(Ctx& c, Idx scanned)
{
    c.budget -= scanned;
}

void addSpan(Ctx& c, Idx start, Idx end, SpanKind kind, int level = 0)
{
    if (end > start) {
        c.out->push_back(Span{static_cast<int>(start), static_cast<int>(end - start), kind, level});
    }
}

bool isBlank(QChar ch)
{
    return ch == u' ' || ch == u'\t';
}

Idx skipBlanks(QStringView line, Idx pos)
{
    while (pos < line.size() && isBlank(line[pos])) {
        ++pos;
    }
    return pos;
}

// Width of the blanks starting at `pos` in columns (a tab counts as 4) and the position after.
struct Indent {
    Idx columns = 0;
    Idx pos = 0;
};

Indent measureIndent(QStringView line, Idx pos)
{
    Indent r{0, pos};
    while (r.pos < line.size() && isBlank(line[r.pos])) {
        r.columns += (line[r.pos] == u'\t') ? 4 : 1;
        ++r.pos;
    }
    return r;
}

Idx runLength(QStringView line, Idx pos, Idx to, char16_t ch)
{
    Idx n = 0;
    while (pos + n < to && line[pos + n].unicode() == ch) {
        ++n;
    }
    return n;
}

char32_t codePointBefore(QStringView line, Idx pos)
{
    const QChar last = line[pos - 1];
    if (last.isLowSurrogate() && pos >= 2 && line[pos - 2].isHighSurrogate()) {
        return QChar::surrogateToUcs4(line[pos - 2], last);
    }
    return last.unicode();
}

bool isTagChar(char32_t cp)
{
    return QChar::isLetterOrNumber(cp) || cp == U'_' || cp == U'-' || cp == U'/';
}

// ---------------------------------------------------------------- inline

void lexInline(Ctx& c, Idx from, Idx to, int depth);

// Returns the end (exclusive) of the code span starting at `pos`, or -1 if it is not closed.
Idx findCodeSpanEnd(Ctx& c, Idx pos, Idx to)
{
    const QStringView line = c.line;
    const Idx limit = searchLimit(c, pos, to);
    const Idx n = runLength(line, pos, limit, u'`');
    Idx j = pos + n;
    Idx result = -1;
    while (n > 0 && j < limit) {
        if (line[j] == u'`') {
            const Idx m = runLength(line, j, limit, u'`');
            if (m == n) {
                result = j + m;
                break;
            }
            j += m;
        } else {
            ++j;
        }
    }
    chargeSearch(c, j - pos);
    return result;
}

// [[target]], [[target#heading]], [[target|alias]]. Returns the end or -1.
Idx tryWikiLink(Ctx& c, Idx i, Idx to)
{
    const QStringView line = c.line;
    if (i + 1 >= to || line[i + 1] != u'[') {
        return -1;
    }
    Idx close = -1;
    Idx bar = -1;
    const Idx limit = std::min(searchLimit(c, i, to), to - 1);
    Idx j = i + 2;
    for (; j < limit; ++j) {
        const QChar ch = line[j];
        if (ch == u'[') {
            break;
        }
        if (ch == u']') {
            if (line[j + 1] == u']') {
                close = j;
            }
            break;
        }
        if (ch == u'|' && bar < 0) {
            bar = j;
        }
    }
    chargeSearch(c, j - i);
    if (close < 0) {
        return -1;
    }
    const Idx targetEnd = bar >= 0 ? bar : close;
    if (targetEnd <= i + 2) {
        return -1;
    }
    addSpan(c, i, i + 2, SpanKind::WikiLinkBracket);
    addSpan(c, i + 2, targetEnd, SpanKind::WikiLinkTarget);
    if (bar >= 0) {
        addSpan(c, bar, bar + 1, SpanKind::WikiLinkBracket);
        addSpan(c, bar + 1, close, SpanKind::WikiLinkAlias);
    }
    addSpan(c, close, close + 2, SpanKind::WikiLinkBracket);
    return close + 2;
}

// [text](url "optional title"). Returns the end or -1.
Idx tryLink(Ctx& c, Idx i, Idx to)
{
    const QStringView line = c.line;
    Idx textEnd = -1;
    int nesting = 1;
    const Idx textLimit = searchLimit(c, i, to);
    Idx k = i + 1;
    for (; k < textLimit; ++k) {
        const QChar ch = line[k];
        if (ch == u'\\') {
            ++k;
        } else if (ch == u'[') {
            ++nesting;
        } else if (ch == u']' && --nesting == 0) {
            textEnd = k;
            break;
        }
    }
    chargeSearch(c, k - i);
    if (textEnd < 0 || textEnd + 1 >= to || line[textEnd + 1] != u'(') {
        return -1;
    }
    const Idx urlStart = textEnd + 2;
    Idx urlEnd = -1;
    Idx close = -1;
    int parens = 1;
    const Idx urlLimit = searchLimit(c, urlStart, to);
    Idx j = urlStart;
    for (; j < urlLimit; ++j) {
        const QChar ch = line[j];
        if (ch == u'\\') {
            ++j;
        } else if (ch.isSpace()) {
            if (urlEnd < 0) {
                urlEnd = j;
            }
        } else if (ch == u'(') {
            ++parens;
        } else if (ch == u')' && --parens == 0) {
            close = j;
            break;
        }
    }
    chargeSearch(c, j - urlStart);
    if (close < 0) {
        return -1;
    }
    if (urlEnd < 0) {
        urlEnd = close;
    }
    addSpan(c, i, i + 1, SpanKind::LinkMarker);
    addSpan(c, i + 1, textEnd, SpanKind::LinkText);
    addSpan(c, textEnd, textEnd + 2, SpanKind::LinkMarker);
    addSpan(c, urlStart, urlEnd, SpanKind::LinkUrl);
    addSpan(c, close, close + 1, SpanKind::LinkMarker);
    return close + 1;
}

// #tag. Returns the end or -1.
Idx tryTag(Ctx& c, Idx i, Idx to)
{
    const QStringView line = c.line;
    if (i > 0) {
        const char32_t before = codePointBefore(line, i);
        if (QChar::isLetterOrNumber(before) || before == U'_' || before == U'#' || before == U'/' ||
            before == U'&') {
            return -1;
        }
    }
    Idx j = i + 1;
    Idx lastGood = j; // end of the accepted tag text (a trailing '/' is not part of it)
    bool allDigits = true;
    while (j < to) {
        char32_t cp = line[j].unicode();
        Idx step = 1;
        if (line[j].isHighSurrogate() && j + 1 < to && line[j + 1].isLowSurrogate()) {
            cp = QChar::surrogateToUcs4(line[j], line[j + 1]);
            step = 2;
        }
        if (!isTagChar(cp)) {
            break;
        }
        if (!QChar::isDigit(cp)) {
            allDigits = false;
        }
        j += step;
        if (cp != U'/') {
            lastGood = j;
        }
    }
    if (lastGood == i + 1 || allDigits) {
        return -1;
    }
    addSpan(c, i, lastGood, SpanKind::Tag);
    return lastGood;
}

// Finds the closing delimiter run for an emphasis opener; returns its start or -1.
Idx findEmphasisCloser(Ctx& c, Idx after, Idx to, char16_t delim, Idx use)
{
    const QStringView line = c.line;
    const Idx limit = searchLimit(c, after, to);
    Idx result = -1;
    Idx j = after;
    while (j < limit) {
        const QChar ch = line[j];
        if (ch == u'\\') {
            j += 2;
        } else if (ch == u'`') {
            // A code span skips its content; its own search is charged to the budget too.
            const Idx end = findCodeSpanEnd(c, j, to);
            j = end > 0 ? end : j + std::max<Idx>(runLength(line, j, limit, u'`'), 1);
        } else if (ch.unicode() == delim) {
            const Idx m = runLength(line, j, limit, delim);
            const bool lenOk = use == 3 ? m >= 3 : m == use;
            const bool spaceOk = !line[j - 1].isSpace();
            const bool wordOk =
                delim != u'_' || j + m >= to || !QChar::isLetterOrNumber(line[j + m].unicode());
            if (lenOk && spaceOk && wordOk) {
                result = j;
                break;
            }
            j += m;
        } else {
            ++j;
        }
    }
    chargeSearch(c, std::min(j, limit) - after);
    return result;
}

// *em*, **strong**, ***both***, and the '_' variants. Returns the end or -1.
Idx tryEmphasis(Ctx& c, Idx i, Idx to, int depth)
{
    const QStringView line = c.line;
    const char16_t delim = line[i].unicode();
    const Idx run = runLength(line, i, to, delim);
    const Idx after = i + run;
    if (after >= to || line[after].isSpace()) {
        return -1;
    }
    if (delim == u'_' && i > 0 && QChar::isLetterOrNumber(codePointBefore(line, i))) {
        return -1;
    }
    const Idx use = std::min<Idx>(run, 3);
    const Idx open = after - use;
    const Idx close = findEmphasisCloser(c, after, to, delim, use);
    if (close < 0) {
        return -1;
    }

    const Idx closeEnd = close + use;
    if (use == 1) {
        addSpan(c, open, open + 1, SpanKind::EmphasisMarker);
        addSpan(c, open + 1, close, SpanKind::EmphasisText);
    } else if (use == 2) {
        addSpan(c, open, open + 2, SpanKind::StrongMarker);
        addSpan(c, open + 2, close, SpanKind::StrongText);
    } else {
        addSpan(c, open, open + 1, SpanKind::EmphasisMarker);
        addSpan(c, open + 1, open + 3, SpanKind::StrongMarker);
        addSpan(c, open + 3, close, SpanKind::EmphasisText);
        addSpan(c, open + 3, close, SpanKind::StrongText);
    }
    if (depth < kMaxInlineDepth) {
        lexInline(c, open + use, close, depth + 1);
    }
    if (use == 1) {
        addSpan(c, close, closeEnd, SpanKind::EmphasisMarker);
    } else if (use == 2) {
        addSpan(c, close, closeEnd, SpanKind::StrongMarker);
    } else {
        addSpan(c, close, close + 2, SpanKind::StrongMarker);
        addSpan(c, close + 2, closeEnd, SpanKind::EmphasisMarker);
    }
    return closeEnd;
}

void lexInline(Ctx& c, Idx from, Idx to, int depth)
{
    const QStringView line = c.line;
    Idx i = from;
    while (i < to) {
        Idx next = -1;
        switch (line[i].unicode()) {
        case u'\\':
            next = i + 2;
            break;
        case u'`':
            next = findCodeSpanEnd(c, i, to);
            if (next > 0) {
                addSpan(c, i, next, SpanKind::InlineCode);
            } else {
                next = i + runLength(line, i, to, u'`');
            }
            break;
        case u'[':
            next = tryWikiLink(c, i, to);
            if (next < 0) {
                next = tryLink(c, i, to);
            }
            break;
        case u'#':
            next = tryTag(c, i, to);
            break;
        case u'*':
        case u'_':
            next = tryEmphasis(c, i, to, depth);
            if (next < 0) {
                next = i + runLength(line, i, to, line[i].unicode());
            }
            break;
        default:
            break;
        }
        i = next > i ? next : i + 1;
    }
}

// ---------------------------------------------------------------- blocks

LineResult lexInsideFence(QStringView line, LexState prev)
{
    LineResult r;
    r.next = prev;
    Ctx c{line, &r.spans, kSearchBudgetPerUnit * line.size() + kSearchBudgetBase};
    const Indent ind = measureIndent(line, 0);
    if (ind.columns <= kMaxIndentForBlocks) {
        const Idx run = runLength(line, ind.pos, line.size(), prev.fenceChar);
        if (run >= prev.fenceLength && skipBlanks(line, ind.pos + run) >= line.size()) {
            addSpan(c, ind.pos, ind.pos + run, SpanKind::FenceDelimiter);
            r.next = LexState{};
            return r;
        }
    }
    addSpan(c, 0, line.size(), SpanKind::CodeBlockText);
    return r;
}

// Opening fence: up to 3 blanks, then 3+ backticks (no backtick in the info string) or tildes.
bool tryOpenFence(Ctx& c, LineResult& r)
{
    const QStringView line = c.line;
    const Indent ind = measureIndent(line, 0);
    if (ind.columns > kMaxIndentForBlocks || ind.pos >= line.size()) {
        return false;
    }
    const char16_t fenceChar = line[ind.pos].unicode();
    if (fenceChar != u'`' && fenceChar != u'~') {
        return false;
    }
    const Idx run = runLength(line, ind.pos, line.size(), fenceChar);
    if (run < 3) {
        return false;
    }
    if (fenceChar == u'`' && line.sliced(ind.pos + run).contains(u'`')) {
        return false;
    }
    addSpan(c, ind.pos, line.size(), SpanKind::FenceDelimiter);
    r.next.inFence = true;
    r.next.fenceChar = fenceChar;
    r.next.fenceLength = static_cast<int>(std::min<Idx>(run, kMaxFenceLength));
    return true;
}

bool tryHorizontalRule(Ctx& c, Idx pos)
{
    const QStringView line = c.line;
    const char16_t ch = line[pos].unicode();
    if (ch != u'-' && ch != u'*' && ch != u'_') {
        return false;
    }
    Idx count = 0;
    Idx lastMark = pos;
    for (Idx j = pos; j < line.size(); ++j) {
        if (line[j].unicode() == ch) {
            ++count;
            lastMark = j;
        } else if (!isBlank(line[j])) {
            return false;
        }
    }
    if (count < 3) {
        return false;
    }
    addSpan(c, pos, lastMark + 1, SpanKind::HorizontalRule);
    return true;
}

bool tryHeading(Ctx& c, Idx pos)
{
    const QStringView line = c.line;
    const Idx hashes = runLength(line, pos, line.size(), u'#');
    if (hashes < 1 || hashes > 6) {
        return false;
    }
    const Idx afterHashes = pos + hashes;
    if (afterHashes < line.size() && !isBlank(line[afterHashes])) {
        return false;
    }
    const int level = static_cast<int>(hashes);
    addSpan(c, pos, afterHashes, SpanKind::HeadingMarker, level);
    const Idx textStart = skipBlanks(line, afterHashes);
    if (textStart < line.size()) {
        addSpan(c, textStart, line.size(), SpanKind::HeadingText, level);
        lexInline(c, textStart, line.size(), 0);
    }
    return true;
}

bool isAsciiDigit(QChar ch)
{
    return ch >= u'0' && ch <= u'9';
}

// Returns the end of the list marker (including a task checkbox), or -1.
Idx findListMarkerEnd(QStringView line, Idx pos)
{
    Idx end = -1;
    const QChar ch = line[pos];
    if (ch == u'-' || ch == u'*' || ch == u'+') {
        end = pos + 1;
    } else if (isAsciiDigit(ch)) {
        Idx j = pos;
        while (j < line.size() && isAsciiDigit(line[j])) {
            ++j;
        }
        if (j - pos > 9 || j >= line.size() || (line[j] != u'.' && line[j] != u')')) {
            return -1;
        }
        end = j + 1;
    } else {
        return -1;
    }
    if (end < line.size() && !isBlank(line[end])) {
        return -1;
    }
    const Idx box = skipBlanks(line, end);
    if (box > end && box + 2 < line.size() && line[box] == u'[' && line[box + 2] == u']' &&
        (line[box + 1] == u' ' || line[box + 1] == u'x' || line[box + 1] == u'X') &&
        (box + 3 >= line.size() || isBlank(line[box + 3]))) {
        return box + 3;
    }
    return end;
}

} // namespace

int LexState::toInt() const
{
    if (!inFence) {
        return 0;
    }
    const int len = std::clamp(fenceLength, 0, kMaxFenceLength);
    return 1 | (fenceChar == u'~' ? 2 : 0) | (len << 2);
}

LexState LexState::fromInt(int value)
{
    LexState s;
    if (value > 0 && (value & 1) != 0) {
        s.inFence = true;
        s.fenceChar = (value & 2) != 0 ? u'~' : u'`';
        s.fenceLength = value >> 2;
    }
    return s;
}

LineResult lexLine(QStringView line, LexState prev)
{
    if (prev.inFence) {
        return lexInsideFence(line, prev);
    }

    LineResult r;
    Ctx c{line, &r.spans, kSearchBudgetPerUnit * line.size() + kSearchBudgetBase};
    if (tryOpenFence(c, r)) {
        return r;
    }

    // Block quote markers may nest: "> > text".
    Idx pos = 0;
    for (;;) {
        const Indent quote = measureIndent(line, pos);
        if (quote.columns > kMaxIndentForBlocks || quote.pos >= line.size() ||
            line[quote.pos] != u'>') {
            break;
        }
        addSpan(c, quote.pos, quote.pos + 1, SpanKind::QuoteMarker);
        pos = quote.pos + 1;
        if (pos < line.size() && line[pos] == u' ') {
            ++pos;
        }
    }

    const Indent ind = measureIndent(line, pos);
    if (ind.pos >= line.size()) {
        return r;
    }
    if (tryHorizontalRule(c, ind.pos)) {
        return r;
    }
    if (ind.columns <= kMaxIndentForBlocks && tryHeading(c, ind.pos)) {
        return r;
    }

    const Idx markerEnd = findListMarkerEnd(line, ind.pos);
    if (markerEnd > 0) {
        addSpan(c, ind.pos, markerEnd, SpanKind::ListMarker);
        lexInline(c, markerEnd, line.size(), 0);
    } else {
        lexInline(c, ind.pos, line.size(), 0);
    }
    return r;
}

} // namespace nexus::parser
