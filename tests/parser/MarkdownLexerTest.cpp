#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <gtest/gtest.h>
#include <iostream>

#include "parser/MarkdownLexer.h"

using nexus::parser::lexLine;
using nexus::parser::LexState;
using nexus::parser::LineResult;
using nexus::parser::Span;
using nexus::parser::SpanKind;

namespace {

// Texts covered by the spans of `kind` in `line`, in output order.
QStringList textsOf(const QString& line, SpanKind kind, LexState prev = {})
{
    QStringList out;
    for (const Span& s : lexLine(line, prev).spans) {
        if (s.kind == kind) {
            out << line.mid(s.start, s.length);
        }
    }
    return out;
}

int countOf(const QString& line, SpanKind kind, LexState prev = {})
{
    return static_cast<int>(textsOf(line, kind, prev).size());
}

// Lexes a whole document line by line, carrying the state like the UI does.
std::vector<LineResult> lexDocument(const QStringList& lines)
{
    std::vector<LineResult> out;
    LexState state;
    for (const QString& line : lines) {
        out.push_back(lexLine(line, state));
        state = LexState::fromInt(out.back().next.toInt());
    }
    return out;
}

LexState inFence(char16_t ch = u'`', int len = 3)
{
    LexState s;
    s.inFence = true;
    s.fenceChar = ch;
    s.fenceLength = len;
    return s;
}

} // namespace

// ------------------------------------------------------------ headings

TEST(MarkdownLexer, HeadingSplitsMarkerAndText)
{
    const QString line = QStringLiteral("## Title");
    const auto r = lexLine(line, {});
    ASSERT_EQ(r.spans.size(), 2U);
    EXPECT_EQ(r.spans[0], (Span{0, 2, SpanKind::HeadingMarker, 2}));
    EXPECT_EQ(r.spans[1], (Span{3, 5, SpanKind::HeadingText, 2}));
}

TEST(MarkdownLexer, HeadingLevelsOneToSix)
{
    for (int level = 1; level <= 6; ++level) {
        const QString line = QString(level, u'#') + QStringLiteral(" x");
        const auto r = lexLine(line, {});
        ASSERT_GE(r.spans.size(), 2U) << level;
        EXPECT_EQ(r.spans[0].level, level);
        EXPECT_EQ(r.spans[1].level, level);
    }
}

TEST(MarkdownLexer, SevenHashesIsNotAHeading)
{
    EXPECT_EQ(countOf(QStringLiteral("####### x"), SpanKind::HeadingMarker), 0);
}

TEST(MarkdownLexer, HashWithoutSpaceIsNotAHeading)
{
    EXPECT_EQ(countOf(QStringLiteral("#title"), SpanKind::HeadingMarker), 0);
}

TEST(MarkdownLexer, HeadingWithIndentUpToThree)
{
    EXPECT_EQ(countOf(QStringLiteral("   # x"), SpanKind::HeadingMarker), 1);
    EXPECT_EQ(countOf(QStringLiteral("    # x"), SpanKind::HeadingMarker), 0);
}

TEST(MarkdownLexer, EmptyHeadingHasOnlyMarker)
{
    const auto r = lexLine(QStringLiteral("# "), {});
    ASSERT_EQ(r.spans.size(), 1U);
    EXPECT_EQ(r.spans[0].kind, SpanKind::HeadingMarker);
}

TEST(MarkdownLexer, HeadingTextContainsInlineSpans)
{
    const QString line = QStringLiteral("# Plan [[Note]] #todo");
    EXPECT_EQ(textsOf(line, SpanKind::WikiLinkTarget), QStringList{QStringLiteral("Note")});
    EXPECT_EQ(textsOf(line, SpanKind::Tag), QStringList{QStringLiteral("#todo")});
}

// ------------------------------------------------------------ lists and quotes

TEST(MarkdownLexer, BulletMarkers)
{
    for (const char* marker : {"-", "*", "+"}) {
        const QString line = QString::fromLatin1(marker) + QStringLiteral(" item");
        EXPECT_EQ(textsOf(line, SpanKind::ListMarker), QStringList{QString::fromLatin1(marker)});
    }
}

TEST(MarkdownLexer, OrderedMarkers)
{
    EXPECT_EQ(textsOf(QStringLiteral("1. a"), SpanKind::ListMarker),
              QStringList{QStringLiteral("1.")});
    EXPECT_EQ(textsOf(QStringLiteral("12) a"), SpanKind::ListMarker),
              QStringList{QStringLiteral("12)")});
}

TEST(MarkdownLexer, TaskMarkersIncludeCheckbox)
{
    EXPECT_EQ(textsOf(QStringLiteral("- [ ] todo"), SpanKind::ListMarker),
              QStringList{QStringLiteral("- [ ]")});
    EXPECT_EQ(textsOf(QStringLiteral("- [x] done"), SpanKind::ListMarker),
              QStringList{QStringLiteral("- [x]")});
    EXPECT_EQ(textsOf(QStringLiteral("* [X] done"), SpanKind::ListMarker),
              QStringList{QStringLiteral("* [X]")});
}

TEST(MarkdownLexer, IndentedNestedListMarker)
{
    const auto r = lexLine(QStringLiteral("      - nested"), {});
    ASSERT_FALSE(r.spans.empty());
    EXPECT_EQ(r.spans[0], (Span{6, 1, SpanKind::ListMarker, 0}));
}

TEST(MarkdownLexer, DashWithoutSpaceIsNotAList)
{
    EXPECT_EQ(countOf(QStringLiteral("-item"), SpanKind::ListMarker), 0);
    EXPECT_EQ(countOf(QStringLiteral("2024-01-01"), SpanKind::ListMarker), 0);
}

TEST(MarkdownLexer, ListItemContentIsLexedInline)
{
    EXPECT_EQ(textsOf(QStringLiteral("- see [[A]] and `x`"), SpanKind::InlineCode),
              QStringList{QStringLiteral("`x`")});
}

TEST(MarkdownLexer, QuoteMarker)
{
    EXPECT_EQ(textsOf(QStringLiteral("> quoted"), SpanKind::QuoteMarker),
              QStringList{QStringLiteral(">")});
}

TEST(MarkdownLexer, NestedQuoteMarkers)
{
    EXPECT_EQ(countOf(QStringLiteral("> > deep"), SpanKind::QuoteMarker), 2);
}

TEST(MarkdownLexer, QuoteContainingListAndInline)
{
    const QString line = QStringLiteral("> - **b**");
    EXPECT_EQ(countOf(line, SpanKind::QuoteMarker), 1);
    EXPECT_EQ(countOf(line, SpanKind::ListMarker), 1);
    EXPECT_EQ(textsOf(line, SpanKind::StrongText), QStringList{QStringLiteral("b")});
}

TEST(MarkdownLexer, HorizontalRules)
{
    for (const char* rule : {"---", "***", "___", "- - -", "*****", "  ---  "}) {
        const QString line = QString::fromLatin1(rule);
        EXPECT_EQ(countOf(line, SpanKind::HorizontalRule), 1) << rule;
        EXPECT_EQ(countOf(line, SpanKind::ListMarker), 0) << rule;
    }
}

TEST(MarkdownLexer, TwoDashesAreNotARule)
{
    EXPECT_EQ(countOf(QStringLiteral("--"), SpanKind::HorizontalRule), 0);
}

// ------------------------------------------------------------ code blocks

TEST(MarkdownLexer, OpeningFenceStartsCodeBlock)
{
    const auto r = lexLine(QStringLiteral("```cpp"), {});
    ASSERT_EQ(r.spans.size(), 1U);
    EXPECT_EQ(r.spans[0], (Span{0, 6, SpanKind::FenceDelimiter, 0}));
    EXPECT_TRUE(r.next.inFence);
    EXPECT_EQ(r.next.fenceChar, u'`');
    EXPECT_EQ(r.next.fenceLength, 3);
}

TEST(MarkdownLexer, TildeFence)
{
    const auto r = lexLine(QStringLiteral("~~~~ text"), {});
    EXPECT_TRUE(r.next.inFence);
    EXPECT_EQ(r.next.fenceChar, u'~');
    EXPECT_EQ(r.next.fenceLength, 4);
}

TEST(MarkdownLexer, BacktickFenceInfoStringCannotContainBacktick)
{
    const auto r = lexLine(QStringLiteral("```a`b"), {});
    EXPECT_FALSE(r.next.inFence);
}

TEST(MarkdownLexer, TwoBackticksAreNotAFence)
{
    EXPECT_FALSE(lexLine(QStringLiteral("``"), {}).next.inFence);
}

TEST(MarkdownLexer, CodeBlockLineIsOpaque)
{
    const QString line = QStringLiteral("# not heading [[x]] **y** #tag `z`");
    const auto r = lexLine(line, inFence());
    ASSERT_EQ(r.spans.size(), 1U);
    EXPECT_EQ(r.spans[0], (Span{0, static_cast<int>(line.size()), SpanKind::CodeBlockText, 0}));
    EXPECT_TRUE(r.next.inFence);
}

TEST(MarkdownLexer, EmptyLineInsideCodeBlockHasNoSpans)
{
    const auto r = lexLine(QString(), inFence());
    EXPECT_TRUE(r.spans.empty());
    EXPECT_TRUE(r.next.inFence);
}

TEST(MarkdownLexer, ClosingFenceEndsCodeBlock)
{
    const auto r = lexLine(QStringLiteral("```"), inFence());
    ASSERT_EQ(r.spans.size(), 1U);
    EXPECT_EQ(r.spans[0].kind, SpanKind::FenceDelimiter);
    EXPECT_FALSE(r.next.inFence);
}

TEST(MarkdownLexer, ClosingFenceMayBeLonger)
{
    EXPECT_FALSE(lexLine(QStringLiteral("`````"), inFence(u'`', 3)).next.inFence);
}

TEST(MarkdownLexer, ClosingFenceCannotBeShorter)
{
    const auto r = lexLine(QStringLiteral("```"), inFence(u'`', 4));
    EXPECT_TRUE(r.next.inFence);
    EXPECT_EQ(r.spans[0].kind, SpanKind::CodeBlockText);
}

TEST(MarkdownLexer, ClosingFenceMustUseSameCharacter)
{
    EXPECT_TRUE(lexLine(QStringLiteral("~~~"), inFence(u'`', 3)).next.inFence);
    EXPECT_TRUE(lexLine(QStringLiteral("```"), inFence(u'~', 3)).next.inFence);
}

TEST(MarkdownLexer, ClosingFenceCannotHaveInfoString)
{
    EXPECT_TRUE(lexLine(QStringLiteral("``` cpp"), inFence()).next.inFence);
}

TEST(MarkdownLexer, ClosingFenceAllowsTrailingBlanks)
{
    EXPECT_FALSE(lexLine(QStringLiteral("```   "), inFence()).next.inFence);
}

TEST(MarkdownLexer, FenceIndentOfFourIsCodeText)
{
    EXPECT_TRUE(lexLine(QStringLiteral("    ```"), inFence()).next.inFence);
}

TEST(MarkdownLexer, FourBacktickFenceKeepsInnerTripleBacktickAsText)
{
    const auto doc =
        lexDocument({QStringLiteral("````md"), QStringLiteral("```"), QStringLiteral("inner"),
                     QStringLiteral("````"), QStringLiteral("# after")});
    EXPECT_EQ(doc[1].spans[0].kind, SpanKind::CodeBlockText);
    EXPECT_EQ(doc[2].spans[0].kind, SpanKind::CodeBlockText);
    EXPECT_EQ(doc[3].spans[0].kind, SpanKind::FenceDelimiter);
    EXPECT_EQ(doc[4].spans[0].kind, SpanKind::HeadingMarker);
}

TEST(MarkdownLexer, UnclosedFenceRunsToTheEnd)
{
    const auto doc = lexDocument({QStringLiteral("```"), QStringLiteral("a"), QStringLiteral("b")});
    EXPECT_TRUE(doc.back().next.inFence);
}

// ------------------------------------------------------------ LexState

TEST(LexState, DefaultEncodesAsZero)
{
    EXPECT_EQ(LexState{}.toInt(), 0);
    EXPECT_EQ(LexState::fromInt(0), LexState{});
}

TEST(LexState, MinusOneMeansDefault)
{
    // QSyntaxHighlighter reports -1 for a block that has never been highlighted.
    EXPECT_EQ(LexState::fromInt(-1), LexState{});
}

TEST(LexState, RoundTripsFenceCharAndLength)
{
    for (const char16_t ch : {u'`', u'~'}) {
        for (const int len : {3, 4, 5, 10, 1000}) {
            const LexState s = inFence(ch, len);
            const LexState back = LexState::fromInt(s.toInt());
            EXPECT_EQ(back, s) << len;
            EXPECT_TRUE(s.toInt() > 0);
        }
    }
}

TEST(LexState, DifferentFencesEncodeDifferently)
{
    EXPECT_NE(inFence(u'`', 3).toInt(), inFence(u'~', 3).toInt());
    EXPECT_NE(inFence(u'`', 3).toInt(), inFence(u'`', 4).toInt());
}

TEST(LexState, HugeFenceLengthIsClamped)
{
    const LexState back = LexState::fromInt(inFence(u'`', 2000000000).toInt());
    EXPECT_TRUE(back.inFence);
    EXPECT_GT(back.fenceLength, 0);
}

TEST(LexState, ResumingFromIntContinuesTheBlock)
{
    const auto open = lexLine(QStringLiteral("~~~~"), {});
    const LexState resumed = LexState::fromInt(open.next.toInt());
    EXPECT_TRUE(lexLine(QStringLiteral("~~~"), resumed).next.inFence);
    EXPECT_FALSE(lexLine(QStringLiteral("~~~~~"), resumed).next.inFence);
}

// ------------------------------------------------------------ inline code

TEST(MarkdownLexer, InlineCode)
{
    EXPECT_EQ(textsOf(QStringLiteral("a `code` b"), SpanKind::InlineCode),
              QStringList{QStringLiteral("`code`")});
}

TEST(MarkdownLexer, InlineCodeSuppressesOtherMarkup)
{
    const QString line = QStringLiteral("`[[x]] **b** #tag [a](u)`");
    EXPECT_EQ(countOf(line, SpanKind::InlineCode), 1);
    EXPECT_EQ(line.size(), lexLine(line, {}).spans[0].length);
    EXPECT_EQ(lexLine(line, {}).spans.size(), 1U);
}

TEST(MarkdownLexer, DoubleBacktickCodeMayContainSingleBacktick)
{
    EXPECT_EQ(textsOf(QStringLiteral("``a`b``"), SpanKind::InlineCode),
              QStringList{QStringLiteral("``a`b``")});
}

TEST(MarkdownLexer, UnclosedBacktickIsPlain)
{
    EXPECT_EQ(countOf(QStringLiteral("a `b"), SpanKind::InlineCode), 0);
}

TEST(MarkdownLexer, StarsInsideCodeDoNotPairWithOutside)
{
    EXPECT_EQ(countOf(QStringLiteral("*a `b*` c"), SpanKind::EmphasisText), 0);
}

// ------------------------------------------------------------ emphasis

TEST(MarkdownLexer, Strong)
{
    const QString line = QStringLiteral("x **bold** y");
    EXPECT_EQ(textsOf(line, SpanKind::StrongMarker),
              (QStringList{QStringLiteral("**"), QStringLiteral("**")}));
    EXPECT_EQ(textsOf(line, SpanKind::StrongText), QStringList{QStringLiteral("bold")});
}

TEST(MarkdownLexer, StrongWithUnderscores)
{
    EXPECT_EQ(textsOf(QStringLiteral("__bold__"), SpanKind::StrongText),
              QStringList{QStringLiteral("bold")});
}

TEST(MarkdownLexer, Emphasis)
{
    const QString line = QStringLiteral("x *it* y");
    EXPECT_EQ(textsOf(line, SpanKind::EmphasisMarker),
              (QStringList{QStringLiteral("*"), QStringLiteral("*")}));
    EXPECT_EQ(textsOf(line, SpanKind::EmphasisText), QStringList{QStringLiteral("it")});
}

TEST(MarkdownLexer, EmphasisWithUnderscore)
{
    EXPECT_EQ(textsOf(QStringLiteral("_it_"), SpanKind::EmphasisText),
              QStringList{QStringLiteral("it")});
}

TEST(MarkdownLexer, IntrawordUnderscoreIsNotEmphasis)
{
    EXPECT_EQ(countOf(QStringLiteral("snake_case_name"), SpanKind::EmphasisText), 0);
}

TEST(MarkdownLexer, StrongAndEmphasisNest)
{
    const QString line = QStringLiteral("**a *b* c**");
    EXPECT_EQ(textsOf(line, SpanKind::StrongText), QStringList{QStringLiteral("a *b* c")});
    EXPECT_EQ(textsOf(line, SpanKind::EmphasisText), QStringList{QStringLiteral("b")});
}

TEST(MarkdownLexer, TripleStarsAreStrongAndEmphasis)
{
    const QString line = QStringLiteral("***both***");
    EXPECT_EQ(textsOf(line, SpanKind::StrongText), QStringList{QStringLiteral("both")});
    EXPECT_EQ(textsOf(line, SpanKind::EmphasisText), QStringList{QStringLiteral("both")});
}

TEST(MarkdownLexer, UnclosedEmphasisIsPlain)
{
    EXPECT_EQ(lexLine(QStringLiteral("**oops"), {}).spans.size(), 0U);
    EXPECT_EQ(lexLine(QStringLiteral("a * b * c"), {}).spans.size(), 0U);
}

TEST(MarkdownLexer, EmphasisCannotStartWithSpace)
{
    EXPECT_EQ(countOf(QStringLiteral("2 * 3 * 4"), SpanKind::EmphasisText), 0);
}

TEST(MarkdownLexer, EscapedStarIsPlain)
{
    EXPECT_EQ(countOf(QStringLiteral("\\*a\\*"), SpanKind::EmphasisText), 0);
}

TEST(MarkdownLexer, StrongContainsWikiLinkAndTag)
{
    const QString line = QStringLiteral("**see [[A]] #t**");
    EXPECT_EQ(textsOf(line, SpanKind::WikiLinkTarget), QStringList{QStringLiteral("A")});
    EXPECT_EQ(textsOf(line, SpanKind::Tag), QStringList{QStringLiteral("#t")});
}

// ------------------------------------------------------------ wiki links

TEST(MarkdownLexer, WikiLinkBracketsAndTarget)
{
    const QString line = QStringLiteral("a [[Note]] b");
    EXPECT_EQ(textsOf(line, SpanKind::WikiLinkBracket),
              (QStringList{QStringLiteral("[["), QStringLiteral("]]")}));
    EXPECT_EQ(textsOf(line, SpanKind::WikiLinkTarget), QStringList{QStringLiteral("Note")});
}

TEST(MarkdownLexer, WikiLinkAlias)
{
    const QString line = QStringLiteral("[[Target|shown]]");
    EXPECT_EQ(textsOf(line, SpanKind::WikiLinkTarget), QStringList{QStringLiteral("Target")});
    EXPECT_EQ(textsOf(line, SpanKind::WikiLinkAlias), QStringList{QStringLiteral("shown")});
    EXPECT_EQ(countOf(line, SpanKind::WikiLinkBracket), 3); // "[[", "|", "]]"
}

TEST(MarkdownLexer, WikiLinkHeadingStaysInTarget)
{
    EXPECT_EQ(textsOf(QStringLiteral("[[Note#Section]]"), SpanKind::WikiLinkTarget),
              QStringList{QStringLiteral("Note#Section")});
}

TEST(MarkdownLexer, WikiLinkHeadingWithAlias)
{
    const QString line = QStringLiteral("[[Note#Sec|alias]]");
    EXPECT_EQ(textsOf(line, SpanKind::WikiLinkTarget), QStringList{QStringLiteral("Note#Sec")});
    EXPECT_EQ(textsOf(line, SpanKind::WikiLinkAlias), QStringList{QStringLiteral("alias")});
}

TEST(MarkdownLexer, WikiLinkDoesNotProduceTag)
{
    EXPECT_EQ(countOf(QStringLiteral("[[Note#Section]]"), SpanKind::Tag), 0);
}

TEST(MarkdownLexer, EmptyOrUnclosedWikiLinkIsPlain)
{
    EXPECT_EQ(countOf(QStringLiteral("[[]]"), SpanKind::WikiLinkTarget), 0);
    EXPECT_EQ(countOf(QStringLiteral("[[open"), SpanKind::WikiLinkTarget), 0);
    EXPECT_EQ(countOf(QStringLiteral("[[a] b]"), SpanKind::WikiLinkTarget), 0);
}

TEST(MarkdownLexer, TwoWikiLinksOnOneLine)
{
    EXPECT_EQ(textsOf(QStringLiteral("[[A]] and [[B]]"), SpanKind::WikiLinkTarget),
              (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
}

TEST(MarkdownLexer, WikiLinkWithChineseAndSpaces)
{
    EXPECT_EQ(textsOf(QStringLiteral("見 [[專案 計畫]]"), SpanKind::WikiLinkTarget),
              QStringList{QStringLiteral("專案 計畫")});
}

// ------------------------------------------------------------ tags

TEST(MarkdownLexer, SimpleTag)
{
    EXPECT_EQ(textsOf(QStringLiteral("a #tag b"), SpanKind::Tag),
              QStringList{QStringLiteral("#tag")});
}

TEST(MarkdownLexer, TagAtLineStart)
{
    EXPECT_EQ(textsOf(QStringLiteral("#tag rest"), SpanKind::Tag),
              QStringList{QStringLiteral("#tag")});
}

TEST(MarkdownLexer, ChineseTag)
{
    EXPECT_EQ(textsOf(QStringLiteral("標籤 #規格 與 #設計稿"), SpanKind::Tag),
              (QStringList{QStringLiteral("#規格"), QStringLiteral("#設計稿")}));
}

TEST(MarkdownLexer, TagNeedsNonAlnumBefore)
{
    EXPECT_EQ(countOf(QStringLiteral("a#b"), SpanKind::Tag), 0);
    EXPECT_EQ(countOf(QStringLiteral("中#文"), SpanKind::Tag), 0);
}

TEST(MarkdownLexer, PureNumberIsNotATag)
{
    EXPECT_EQ(countOf(QStringLiteral("issue #123 fixed"), SpanKind::Tag), 0);
    EXPECT_EQ(countOf(QStringLiteral("see #1a"), SpanKind::Tag), 1);
}

TEST(MarkdownLexer, HashFollowedBySpaceIsNotATag)
{
    EXPECT_EQ(countOf(QStringLiteral("a # b"), SpanKind::Tag), 0);
    EXPECT_EQ(countOf(QStringLiteral("a #"), SpanKind::Tag), 0);
}

TEST(MarkdownLexer, NestedTagWithSlashAndDash)
{
    EXPECT_EQ(textsOf(QStringLiteral("#project/sub-1 x"), SpanKind::Tag),
              QStringList{QStringLiteral("#project/sub-1")});
}

TEST(MarkdownLexer, TagStopsAtPunctuation)
{
    EXPECT_EQ(textsOf(QStringLiteral("(#tag), #b."), SpanKind::Tag),
              (QStringList{QStringLiteral("#tag"), QStringLiteral("#b")}));
}

TEST(MarkdownLexer, DoubleHashIsNotATag)
{
    EXPECT_EQ(countOf(QStringLiteral("x ##tag"), SpanKind::Tag), 0);
}

TEST(MarkdownLexer, TagStopsBeforeEmoji)
{
    EXPECT_EQ(textsOf(QStringLiteral("#tag😀"), SpanKind::Tag),
              QStringList{QStringLiteral("#tag")});
}

// ------------------------------------------------------------ links

TEST(MarkdownLexer, Link)
{
    const QString line = QStringLiteral("a [text](https://x.y/z) b");
    EXPECT_EQ(textsOf(line, SpanKind::LinkText), QStringList{QStringLiteral("text")});
    EXPECT_EQ(textsOf(line, SpanKind::LinkUrl), QStringList{QStringLiteral("https://x.y/z")});
    EXPECT_EQ(textsOf(line, SpanKind::LinkMarker),
              (QStringList{QStringLiteral("["), QStringLiteral("]("), QStringLiteral(")")}));
}

TEST(MarkdownLexer, LinkUrlWithTitleAndParens)
{
    EXPECT_EQ(textsOf(QStringLiteral("[a](u \"title\")"), SpanKind::LinkUrl),
              QStringList{QStringLiteral("u")});
    EXPECT_EQ(textsOf(QStringLiteral("[a](x_(y))"), SpanKind::LinkUrl),
              QStringList{QStringLiteral("x_(y)")});
}

TEST(MarkdownLexer, UrlFragmentIsNotATag)
{
    EXPECT_EQ(countOf(QStringLiteral("[a](http://x/#frag)"), SpanKind::Tag), 0);
}

TEST(MarkdownLexer, BracketsWithoutUrlArePlain)
{
    EXPECT_EQ(countOf(QStringLiteral("[text] (url)"), SpanKind::LinkText), 0);
    EXPECT_EQ(countOf(QStringLiteral("[text](url"), SpanKind::LinkText), 0);
}

TEST(MarkdownLexer, EmptyLinkTextStillMarksUrl)
{
    EXPECT_EQ(textsOf(QStringLiteral("[](u)"), SpanKind::LinkUrl),
              QStringList{QStringLiteral("u")});
}

// ------------------------------------------------------------ UTF-16 positions

TEST(MarkdownLexer, ChinesePositionsAreCodeUnits)
{
    const auto r = lexLine(QStringLiteral("中文 [[筆記]]"), {});
    bool found = false;
    for (const Span& s : r.spans) {
        if (s.kind == SpanKind::WikiLinkTarget) {
            EXPECT_EQ(s.start, 5);
            EXPECT_EQ(s.length, 2);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(MarkdownLexer, EmojiSurrogatePairsCountAsTwoUnits)
{
    // "😀" is 2 UTF-16 code units, so the wiki link starts at 3 and its target at 5.
    const QString line = QStringLiteral("😀 [[A]]");
    const auto r = lexLine(line, {});
    ASSERT_EQ(r.spans.size(), 3U);
    EXPECT_EQ(r.spans[0], (Span{3, 2, SpanKind::WikiLinkBracket, 0}));
    EXPECT_EQ(r.spans[1], (Span{5, 1, SpanKind::WikiLinkTarget, 0}));
    EXPECT_EQ(r.spans[2], (Span{6, 2, SpanKind::WikiLinkBracket, 0}));
}

TEST(MarkdownLexer, EmojiInsideStrongAndTagsAroundEmoji)
{
    const QString line = QStringLiteral("**😀 ok** 😀#no 😀 #yes");
    EXPECT_EQ(textsOf(line, SpanKind::StrongText), QStringList{QStringLiteral("😀 ok")});
    EXPECT_EQ(textsOf(line, SpanKind::Tag),
              (QStringList{QStringLiteral("#no"), QStringLiteral("#yes")}));
}

TEST(MarkdownLexer, TagAfterEmojiWithoutSpaceIsAllowed)
{
    // An emoji is not alphanumeric, so it does not block a tag.
    EXPECT_EQ(countOf(QStringLiteral("😀#tag"), SpanKind::Tag), 1);
}

TEST(MarkdownLexer, SurrogateLetterBeforeHashBlocksTag)
{
    // U+20000 is a CJK ideograph outside the BMP (a letter), stored as a surrogate pair.
    const QString line = QString::fromUcs4(U"\U00020000#tag");
    EXPECT_EQ(countOf(line, SpanKind::Tag), 0);
}

TEST(MarkdownLexer, SurrogateTagCharsAreAccepted)
{
    const QString line = QString::fromUcs4(U"#\U00020000x");
    const auto r = lexLine(line, {});
    ASSERT_EQ(r.spans.size(), 1U);
    EXPECT_EQ(r.spans[0].length, 4); // '#' + surrogate pair + 'x'
}

// ------------------------------------------------------------ misc / invariants

TEST(MarkdownLexer, EmptyAndBlankLinesHaveNoSpans)
{
    EXPECT_TRUE(lexLine(QString(), {}).spans.empty());
    EXPECT_TRUE(lexLine(QStringLiteral("   \t "), {}).spans.empty());
}

TEST(MarkdownLexer, PlainTextHasNoSpansAndKeepsState)
{
    const auto r = lexLine(QStringLiteral("just words"), {});
    EXPECT_TRUE(r.spans.empty());
    EXPECT_FALSE(r.next.inFence);
}

TEST(MarkdownLexer, SpansAreSortedAndInsideTheLine)
{
    const QStringList lines = {
        QStringLiteral("# H **b** [[x|y]] #t `c` [l](u)"),
        QStringLiteral("> - [ ] ***all*** and _it_ [[a#b]] #中文/標籤"),
        QStringLiteral("  1. text \\* [[A]]"),
    };
    for (const QString& line : lines) {
        int prevStart = 0;
        for (const Span& s : lexLine(line, {}).spans) {
            EXPECT_GE(s.start, prevStart) << line.toStdString();
            EXPECT_GT(s.length, 0);
            EXPECT_LE(s.start + s.length, line.size());
            prevStart = s.start;
        }
    }
}

TEST(MarkdownLexer, GarbageInputNeverCrashes)
{
    const QStringList garbage = {
        QStringLiteral("[[[[[[]]]]]]"),
        QStringLiteral("*****_____`````"),
        QStringLiteral("[](()[]){}"),
        QStringLiteral("\\"),
        QStringLiteral("#"),
        QStringLiteral("> > > > > >"),
        QStringLiteral("- [ ]"),
        QStringLiteral("***a**b*"),
    };
    for (const QString& line : garbage) {
        const auto r = lexLine(line, {});
        for (const Span& s : r.spans) {
            EXPECT_GE(s.start, 0);
            EXPECT_LE(s.start + s.length, line.size());
        }
    }
}

TEST(MarkdownLexer, DeeplyNestedEmphasisIsBounded)
{
    QString line;
    for (int i = 0; i < 200; ++i) {
        line += QStringLiteral("*a ");
    }
    for (int i = 0; i < 200; ++i) {
        line += QStringLiteral("a* ");
    }
    EXPECT_NO_FATAL_FAILURE(lexLine(line, {}));
}

TEST(MarkdownLexer, DocumentWithCodeBlockInTheMiddle)
{
    const auto doc = lexDocument({
        QStringLiteral("# Title"),
        QStringLiteral("text #tag"),
        QStringLiteral("```cpp"),
        QStringLiteral("#include <x> // [[no]]"),
        QStringLiteral("```"),
        QStringLiteral("after [[yes]]"),
    });
    EXPECT_EQ(doc[1].spans[0].kind, SpanKind::Tag);
    EXPECT_EQ(doc[3].spans[0].kind, SpanKind::CodeBlockText);
    EXPECT_EQ(doc[3].spans.size(), 1U);
    EXPECT_EQ(doc[5].spans[1].kind, SpanKind::WikiLinkTarget);
}

// ------------------------------------------------------------ performance

TEST(MarkdownLexerPerf, TenThousandMixedLines)
{
    const QStringList pattern = {
        QStringLiteral("# 標題 heading #tag"),
        QStringLiteral("普通段落文字 with **strong** and *emphasis* and `code` 😀"),
        QStringLiteral("- [ ] task [[Some Note|alias]] and [link](https://example.com/a)"),
        QStringLiteral("> quote line #規格 _it_"),
        QStringLiteral("```cpp"),
        QStringLiteral("int main() { return 0; } // [[not a link]] **no**"),
        QStringLiteral("```"),
        QStringLiteral("1. ordered item ***both*** a_b_c 100 * 3 * 4"),
        QStringLiteral(""),
        QStringLiteral("---"),
    };
    QStringList lines;
    lines.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        lines << pattern[i % pattern.size()];
    }

    QElapsedTimer timer;
    timer.start();
    LexState state;
    std::size_t spanCount = 0;
    for (const QString& line : lines) {
        const LineResult r = lexLine(line, state);
        spanCount += r.spans.size();
        state = LexState::fromInt(r.next.toInt());
    }
    const qint64 ms = timer.elapsed();

    EXPECT_GT(spanCount, 10000U);
    std::cout << "[perf] lexed 10000 lines in " << ms << " ms, " << spanCount << " spans\n";
#ifdef NDEBUG
    EXPECT_LT(ms, 50);
#endif
}
