#include "ai/StreamJsonParser.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QString>

#include <variant>

using namespace nexus::ai;

namespace {

const ParsedLine& ok(const nexus::Expected<ParsedLine, ParseError>& r)
{
    EXPECT_TRUE(r.has_value());
    return *r;
}

template <class T> const T& only(const ParsedLine& p)
{
    EXPECT_EQ(p.events.size(), 1);
    return std::get<T>(p.events.front());
}

} // namespace

TEST(StreamJsonParser, SystemInitGivesSessionInit)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"system","subtype":"init","session_id":"abc","model":"m1","tools":["x"]})");
    const auto& init = only<SessionInit>(ok(r));
    EXPECT_EQ(init.sessionId, "abc");
    EXPECT_EQ(init.model, "m1");
}

TEST(StreamJsonParser, SystemOtherSubtypeIsIgnored)
{
    const auto r = parseStreamJsonLine(R"({"type":"system","subtype":"compact_boundary"})");
    EXPECT_TRUE(ok(r).events.isEmpty());
    EXPECT_EQ(ok(r).ignoredType, "system/compact_boundary");
}

TEST(StreamJsonParser, AssistantTextBlock)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"text","text":"你好 \"world\""}]}})");
    EXPECT_EQ(only<AssistantText>(ok(r)).text, QString::fromUtf8("你好 \"world\""));
}

TEST(StreamJsonParser, AssistantThinkingBlockIsSkipped)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"thinking","thinking":"hmm","signature":"x"}]}})");
    EXPECT_TRUE(ok(r).events.isEmpty());
    EXPECT_TRUE(ok(r).ignoredType.isEmpty());
}

TEST(StreamJsonParser, AssistantMultipleBlocksKeepOrder)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"text","text":"a"},{"type":"tool_use","id":"t1","name":"Read","input":{"file_path":"/x/y.cpp"}},{"type":"text","text":"b"}]}})");
    const auto& p = ok(r);
    ASSERT_EQ(p.events.size(), 3);
    EXPECT_TRUE(std::holds_alternative<AssistantText>(p.events[0]));
    EXPECT_TRUE(std::holds_alternative<ToolUse>(p.events[1]));
    EXPECT_EQ(std::get<AssistantText>(p.events[2]).text, "b");
}

TEST(StreamJsonParser, ToolUseSummaryPrefersCommand)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"tool_use","id":"t1","name":"Bash","input":{"description":"d","command":"echo hi"}}]}})");
    const auto& use = only<ToolUse>(ok(r));
    EXPECT_EQ(use.name, "Bash");
    EXPECT_EQ(use.toolUseId, "t1");
    EXPECT_EQ(use.inputSummary, "echo hi");
}

TEST(StreamJsonParser, ToolUseSummaryFallsBackToCompactJson)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"tool_use","id":"t","name":"X","input":{"a":1,"b":[true]}}]}})");
    EXPECT_EQ(only<ToolUse>(ok(r)).inputSummary, R"({"a":1,"b":[true]})");
}

TEST(StreamJsonParser, ToolUseWithoutInputHasEmptySummary)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"tool_use","id":"t","name":"X"}]}})");
    EXPECT_TRUE(only<ToolUse>(ok(r)).inputSummary.isEmpty());
}

TEST(StreamJsonParser, LongSummaryIsTruncated)
{
    const QByteArray cmd(1000, 'z');
    const auto r = parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"tool_use","id":"t","name":"Bash","input":{"command":")" +
        cmd + R"("}}]}})");
    const QString s = only<ToolUse>(ok(r)).inputSummary;
    EXPECT_LE(s.size(), 201);
    EXPECT_TRUE(s.endsWith(QChar(0x2026)));
}

TEST(StreamJsonParser, ToolResultStringContent)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"user","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":"t1","content":"hi\nthere","is_error":false}]}})");
    const auto& res = only<ToolResult>(ok(r));
    EXPECT_EQ(res.toolUseId, "t1");
    EXPECT_FALSE(res.isError);
    EXPECT_EQ(res.summary, "hi there");
}

TEST(StreamJsonParser, ToolResultErrorFlag)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"t1","content":"nope","is_error":true}]}})");
    EXPECT_TRUE(only<ToolResult>(ok(r)).isError);
}

TEST(StreamJsonParser, ToolResultArrayContent)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"user","message":{"content":[{"type":"tool_result","tool_use_id":"t1","content":[{"type":"text","text":"one"},{"type":"image"},{"type":"text","text":"two"}]}]}})");
    EXPECT_EQ(only<ToolResult>(ok(r)).summary, "one two");
}

TEST(StreamJsonParser, PlainUserMessageProducesNothing)
{
    const auto r =
        parseStreamJsonLine(R"({"type":"user","message":{"role":"user","content":"hello"}})");
    EXPECT_TRUE(ok(r).events.isEmpty());
}

TEST(StreamJsonParser, ResultSuccess)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"result","subtype":"success","is_error":false,"duration_ms":1234,"total_cost_usd":0.25,"session_id":"s","result":"2"})");
    const auto& res = only<AgentResult>(ok(r));
    EXPECT_TRUE(res.success);
    EXPECT_EQ(res.durationMs, 1234);
    ASSERT_TRUE(res.costUsd.has_value());
    EXPECT_DOUBLE_EQ(*res.costUsd, 0.25);
    EXPECT_EQ(res.sessionId, "s");
    EXPECT_EQ(res.finalText, "2");
}

TEST(StreamJsonParser, ResultErrorWithErrorsArray)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"result","subtype":"error_max_turns","is_error":true,"errors":["a","b"]})");
    const auto& res = only<AgentResult>(ok(r));
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.finalText, "a; b");
    EXPECT_FALSE(res.costUsd.has_value());
}

TEST(StreamJsonParser, ResultIsErrorOverridesSuccessSubtype)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"result","subtype":"success","is_error":true,"result":"API Error"})");
    EXPECT_FALSE(only<AgentResult>(ok(r)).success);
}

TEST(StreamJsonParser, ResultWithoutTextUsesSubtypeWhenFailed)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"result","subtype":"error_during_execution","is_error":true})");
    EXPECT_EQ(only<AgentResult>(ok(r)).finalText, "error_during_execution");
}

TEST(StreamJsonParser, ResultDurationFallsBackToApiDuration)
{
    const auto r =
        parseStreamJsonLine(R"({"type":"result","subtype":"success","duration_api_ms":77})");
    EXPECT_EQ(only<AgentResult>(ok(r)).durationMs, 77);
}

TEST(StreamJsonParser, UnknownTypeIsIgnoredNotAnError)
{
    const auto r = parseStreamJsonLine(R"({"type":"rate_limit_event","rate_limit_info":{"a":1}})");
    EXPECT_TRUE(ok(r).events.isEmpty());
    EXPECT_EQ(ok(r).ignoredType, "rate_limit_event");
}

TEST(StreamJsonParser, MissingTypeIsIgnored)
{
    const auto r = parseStreamJsonLine(R"({"hello":"world"})");
    EXPECT_TRUE(ok(r).events.isEmpty());
    EXPECT_FALSE(ok(r).ignoredType.isEmpty());
}

TEST(StreamJsonParser, UnknownFieldsAndWrongTypesAreTolerated)
{
    const auto r = parseStreamJsonLine(
        R"({"type":"assistant","message":{"content":[{"type":"text","text":5},{"type":"tool_use","id":7,"name":null,"input":"str"}],"zzz":{}},"extra":[1]})");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->events.size(), 2);
}

TEST(StreamJsonParser, MessageWithoutContentIsEmpty)
{
    EXPECT_TRUE(ok(parseStreamJsonLine(R"({"type":"assistant"})")).events.isEmpty());
    EXPECT_TRUE(ok(parseStreamJsonLine(R"({"type":"assistant","message":{"content":"str"}})"))
                    .events.isEmpty());
}

TEST(StreamJsonParser, MalformedJsonIsAnError)
{
    EXPECT_FALSE(parseStreamJsonLine("{not json").has_value());
    EXPECT_FALSE(parseStreamJsonLine(R"({"type":"result",)").has_value());
    EXPECT_FALSE(parseStreamJsonLine("").has_value());
    EXPECT_FALSE(parseStreamJsonLine("plain text").has_value());
}

TEST(StreamJsonParser, NonObjectJsonIsAnError)
{
    EXPECT_FALSE(parseStreamJsonLine("[1,2,3]").has_value());
    EXPECT_FALSE(parseStreamJsonLine("42").has_value());
    EXPECT_FALSE(parseStreamJsonLine("\"str\"").has_value());
    EXPECT_FALSE(parseStreamJsonLine("null").has_value());
}

TEST(StreamJsonParser, ErrorKeepsTruncatedOffendingLine)
{
    const auto r = parseStreamJsonLine(QByteArray(5000, '{'));
    ASSERT_FALSE(r.has_value());
    EXPECT_LE(r.error().line.size(), 200);
    EXPECT_FALSE(r.error().message.isEmpty());
}

TEST(StreamJsonParser, TruncatedUtf8AndBinaryGarbageDoNotCrash)
{
    EXPECT_FALSE(parseStreamJsonLine(QByteArray("\xff\xfe\x00\x01{", 5)).has_value());
    EXPECT_FALSE(
        parseStreamJsonLine(QByteArray("{\"type\":\"result\",\"result\":\"\xe4\xbd")).has_value());
}

TEST(StreamJsonParser, DeeplyNestedJsonDoesNotCrash)
{
    const QByteArray deep = QByteArray(10000, '[') + QByteArray(10000, ']');
    EXPECT_FALSE(parseStreamJsonLine(deep).has_value()); // too deep or not an object
}

TEST(StreamJsonParser, RealFixtureSimpleAnswer)
{
    QFile f(QStringLiteral(NEXUS_AI_FIXTURE_DIR "/simple_answer.jsonl"));
    ASSERT_TRUE(f.open(QIODevice::ReadOnly));
    LineSplitter splitter;
    int texts = 0;
    int results = 0;
    int inits = 0;
    int ignored = 0;
    for (const QByteArray& line : splitter.feed(f.readAll())) {
        const auto r = parseStreamJsonLine(line);
        ASSERT_TRUE(r.has_value());
        ignored += r->ignoredType.isEmpty() ? 0 : 1;
        for (const AgentEvent& e : r->events) {
            texts += std::holds_alternative<AssistantText>(e) ? 1 : 0;
            results += std::holds_alternative<AgentResult>(e) ? 1 : 0;
            inits += std::holds_alternative<SessionInit>(e) ? 1 : 0;
        }
    }
    EXPECT_EQ(inits, 1);
    EXPECT_EQ(texts, 1);
    EXPECT_EQ(results, 1);
    EXPECT_EQ(ignored, 1); // rate_limit_event
}

// ---- LineSplitter -------------------------------------------------------------------------

TEST(LineSplitter, SplitsCompleteLines)
{
    LineSplitter s;
    const auto lines = s.feed("a\nb\nc\n");
    ASSERT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[2], "c");
    EXPECT_FALSE(s.finish().has_value());
}

TEST(LineSplitter, JoinsLineSplitAcrossChunks)
{
    LineSplitter s;
    EXPECT_TRUE(s.feed("{\"type\":").isEmpty());
    EXPECT_TRUE(s.feed("\"sys").isEmpty());
    const auto lines = s.feed("tem\"}\nnext");
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "{\"type\":\"system\"}");
    EXPECT_EQ(s.finish().value_or(QByteArray()), "next");
}

TEST(LineSplitter, ByteByByteFeedGivesSameLines)
{
    const QByteArray data = "one\r\ntwo\n\nthree\n";
    LineSplitter s;
    QList<QByteArray> lines;
    for (const char c : data) {
        lines += s.feed(QByteArray(1, c));
    }
    ASSERT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], "one");
    EXPECT_EQ(lines[1], "two");
    EXPECT_EQ(lines[2], "three");
}

TEST(LineSplitter, HandlesCrLfAndSkipsEmptyLines)
{
    LineSplitter s;
    const auto lines = s.feed("a\r\n\r\n\nb\r\n");
    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0], "a");
}

TEST(LineSplitter, OversizedLineIsDroppedAndCounted)
{
    LineSplitter s(10);
    EXPECT_TRUE(s.feed("12345678901234").isEmpty()); // exceeds limit without newline
    const auto lines = s.feed("more\nok\n");
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "ok");
    EXPECT_EQ(s.overflowed(), 1);
}

TEST(LineSplitter, OversizedCompleteLineInOneChunk)
{
    LineSplitter s(5);
    const auto lines = s.feed("toolongline\nok\n");
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(s.overflowed(), 1);
}

TEST(LineSplitter, LargeLineInSmallChunksStaysLinear)
{
    LineSplitter s;
    const QByteArray chunk(4096, 'x');
    for (int i = 0; i < 2560; ++i) { // 10 MB, no newline
        EXPECT_TRUE(s.feed(chunk).isEmpty());
    }
    const auto lines = s.feed("\n");
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].size(), 4096 * 2560);
}
