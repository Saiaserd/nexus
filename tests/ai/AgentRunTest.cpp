#include "ai/AgentRun.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>

using namespace nexus::ai;

namespace {

const QString kFixtures = QStringLiteral(NEXUS_AI_FIXTURE_DIR);

struct Outcome {
    bool done = false;
    bool timedOut = false;
    bool startedSignal = false;
    QStringList log; // "text:..", "tool:name|summary", "result:err|summary", "init:id|model"
    AgentResult result;
    bool hasResult = false;
    AgentError error;
    bool hasError = false;
    qint64 elapsedMs = 0;
};

AgentRunConfig fakeConfig(const QStringList& fakeArgs)
{
    AgentRunConfig c;
    c.executable = QStringLiteral(NEXUS_FAKE_CLAUDE_PATH);
    c.extraArgs = fakeArgs;
    c.killGraceMs = 300;
    return c;
}

QStringList replayArgs(const QString& fixture, const QStringList& more = {})
{
    return QStringList{QStringLiteral("--fake=replay"),
                       QStringLiteral("--fake-arg=") + kFixtures + u'/' + fixture} +
           more;
}

// Runs the event loop until finished()/failed() or `limitMs`. `afterStart` may call cancel().
Outcome runAgent(AgentRun& run, const QString& prompt, int limitMs = 30000,
                 const std::function<void()>& afterStart = {})
{
    Outcome out;
    QEventLoop loop;
    QObject::connect(&run, &AgentRun::started, [&] { out.startedSignal = true; });
    QObject::connect(&run, &AgentRun::sessionInitialized,
                     [&](const QString& id, const QString& model) {
                         out.log << QStringLiteral("init:") + id + u'|' + model;
                     });
    QObject::connect(&run, &AgentRun::assistantText,
                     [&](const QString& t) { out.log << QStringLiteral("text:") + t; });
    QObject::connect(&run, &AgentRun::toolUse, [&](const QString& n, const QString& s) {
        out.log << QStringLiteral("tool:") + n + u'|' + s;
    });
    QObject::connect(&run, &AgentRun::toolResult, [&](bool e, const QString& s) {
        out.log << QStringLiteral("result:") + (e ? QStringLiteral("err") : QStringLiteral("ok")) +
                       u'|' + s;
    });
    QObject::connect(&run, &AgentRun::finished, [&](const AgentResult& r) {
        out.result = r;
        out.hasResult = true;
        out.done = true;
        loop.quit();
    });
    QObject::connect(&run, &AgentRun::failed, [&](const AgentError& e) {
        out.error = e;
        out.hasError = true;
        out.done = true;
        loop.quit();
    });
    QTimer limit;
    limit.setSingleShot(true);
    QObject::connect(&limit, &QTimer::timeout, [&] {
        out.timedOut = true;
        loop.quit();
    });
    limit.start(limitMs);

    QElapsedTimer clock;
    clock.start();
    EXPECT_TRUE(run.start(prompt));
    if (afterStart) {
        afterStart();
    }
    loop.exec();
    out.elapsedMs = clock.elapsed();
    return out;
}

} // namespace

TEST(AgentRun, NormalCompletionWithRealFixture)
{
    AgentRun run(fakeConfig(replayArgs(QStringLiteral("simple_answer.jsonl"))));
    const Outcome o = runAgent(run, QStringLiteral("1+1"));
    ASSERT_TRUE(o.hasResult) << (o.hasError ? o.error.message.toStdString() : "timeout");
    EXPECT_TRUE(o.startedSignal);
    EXPECT_TRUE(o.result.success);
    EXPECT_EQ(o.result.finalText, "2");
    EXPECT_FALSE(o.result.sessionId.isEmpty());
    EXPECT_TRUE(o.result.costUsd.has_value());
    ASSERT_GE(o.log.size(), 2);
    EXPECT_TRUE(o.log.first().startsWith("init:"));
    EXPECT_TRUE(o.log.contains("text:2"));
    EXPECT_FALSE(run.isRunning());
    EXPECT_TRUE(run.ignoredTypes().contains("rate_limit_event"));
}

TEST(AgentRun, ToolUseAndToolResultThenMaxTurnsResult)
{
    AgentRun run(fakeConfig(
        replayArgs(QStringLiteral("tool_use_max_turns.jsonl"), {QStringLiteral("--fake-exit=1")})));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    // The CLI exits 1 here, but it delivered a result line: that is reported as finished.
    ASSERT_TRUE(o.hasResult);
    EXPECT_FALSE(o.result.success);
    EXPECT_TRUE(o.result.finalText.contains("maximum number of turns"));
    EXPECT_TRUE(o.log.contains("tool:Bash|echo hi"));
    EXPECT_TRUE(o.log.contains("result:ok|hi"));
}

TEST(AgentRun, CliReportedErrorIsFinishedWithFailure)
{
    AgentRun run(fakeConfig(
        replayArgs(QStringLiteral("cli_error.jsonl"), {QStringLiteral("--fake-exit=1")})));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasResult);
    EXPECT_FALSE(o.result.success);
    EXPECT_EQ(o.result.finalText, "boom; bang");
}

TEST(AgentRun, PromptArgumentsAndWorkingDirectoryReachTheCli)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    AgentRunConfig c = fakeConfig({QStringLiteral("--fake=echo"), QStringLiteral("--extra")});
    c.workingDirectory = dir.path();
    AgentRun run(c);
    const QString prompt = QString::fromUtf8("第一行\nsecond \"line\"");
    const Outcome o = runAgent(run, prompt);
    ASSERT_TRUE(o.hasResult);
    EXPECT_TRUE(o.log.contains("text:prompt=" + prompt));
    const QString args = o.log.at(2);
    EXPECT_TRUE(args.contains("-p|--output-format|stream-json|--verbose|")) << args.toStdString();
    EXPECT_TRUE(args.contains("--extra|"));
    // The reported cwd is the temp dir (compare canonical forms).
    QString cwd;
    for (const QString& l : o.log) {
        if (l.startsWith("text:cwd=")) {
            cwd = l.mid(9);
        }
    }
    EXPECT_EQ(QFileInfo(cwd).canonicalFilePath(), QFileInfo(dir.path()).canonicalFilePath());
}

TEST(AgentRun, OutputSplitIntoTinyChunks)
{
    AgentRun run(fakeConfig(
        replayArgs(QStringLiteral("tool_use_max_turns.jsonl"),
                   {QStringLiteral("--fake-chunk=7"), QStringLiteral("--fake-exit=1")})));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasResult);
    EXPECT_TRUE(o.log.contains("tool:Bash|echo hi"));
    EXPECT_EQ(run.unparsedLines(), 0);
}

TEST(AgentRun, UnknownEventsAreTolerated)
{
    AgentRun run(fakeConfig(replayArgs(QStringLiteral("future_cli.jsonl"))));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasResult);
    EXPECT_TRUE(o.result.success);
    EXPECT_TRUE(o.log.contains("text:hello"));
    EXPECT_TRUE(run.ignoredTypes().contains("brand_new_event"));
    EXPECT_TRUE(run.ignoredTypes().contains("system/future_thing"));
}

TEST(AgentRun, NonZeroExitWithoutResultCarriesStderr)
{
    AgentRun run(fakeConfig({QStringLiteral("--fake=exit"), QStringLiteral("--fake-exit=7")}));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasError);
    EXPECT_EQ(o.error.kind, AgentErrorKind::NonZeroExit);
    EXPECT_EQ(o.error.exitCode, 7);
    EXPECT_TRUE(o.error.stderrText.contains("something went wrong"));
}

TEST(AgentRun, GarbageOutputIsParseError)
{
    AgentRun run(fakeConfig({QStringLiteral("--fake=garbage")}));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasError);
    EXPECT_EQ(o.error.kind, AgentErrorKind::ParseError);
    EXPECT_EQ(run.unparsedLines(), 2);
}

TEST(AgentRun, EmptyOutputWithExitZeroIsParseError)
{
    AgentRun run(fakeConfig(replayArgs(QStringLiteral("empty_does_not_exist.jsonl"))));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasError);
    EXPECT_EQ(o.error.kind, AgentErrorKind::ParseError);
}

TEST(AgentRun, MissingExecutable)
{
    AgentRunConfig c;
    c.executable = QStringLiteral("definitely-not-a-real-claude-binary");
    AgentRun run(c);
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasError);
    EXPECT_EQ(o.error.kind, AgentErrorKind::ExecutableNotFound);
}

TEST(AgentRun, MissingExecutablePath)
{
    AgentRunConfig c;
    c.executable = QStringLiteral("/no/such/dir/claude");
    AgentRun run(c);
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasError);
    EXPECT_EQ(o.error.kind, AgentErrorKind::ExecutableNotFound);
}

TEST(AgentRun, NotExecutableFileFailsToStart)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/claude-not-a-program");
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("just text");
    f.close();
    AgentRunConfig c;
    c.executable = path;
    AgentRun run(c);
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasError);
    EXPECT_EQ(o.error.kind, AgentErrorKind::StartFailed);
}

TEST(AgentRun, TimeoutStopsAHungCli)
{
    AgentRunConfig c = fakeConfig({QStringLiteral("--fake=hang")});
    c.timeoutMs = 400;
    AgentRun run(c);
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasError);
    EXPECT_EQ(o.error.kind, AgentErrorKind::Timeout);
    EXPECT_LT(o.elapsedMs, 10000);
}

TEST(AgentRun, CancelStopsAHungCli)
{
    AgentRun run(fakeConfig({QStringLiteral("--fake=hang")}));
    QTimer::singleShot(300, &run, [&run] { run.cancel(); });
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasError);
    EXPECT_EQ(o.error.kind, AgentErrorKind::Cancelled);
    EXPECT_LT(o.elapsedMs, 10000);
    EXPECT_FALSE(run.isRunning());
}

TEST(AgentRun, CancelKillsACliThatIgnoresTerminate)
{
    AgentRun run(fakeConfig({QStringLiteral("--fake=hang"), QStringLiteral("--fake-ignore-term")}));
    QTimer::singleShot(300, &run, [&run] { run.cancel(); });
    const Outcome o = runAgent(run, QStringLiteral("x"), 20000);
    ASSERT_TRUE(o.hasError) << "timed out waiting for kill";
    EXPECT_EQ(o.error.kind, AgentErrorKind::Cancelled);
    EXPECT_GE(o.elapsedMs, 300 + 250); // had to wait for the kill grace period
}

TEST(AgentRun, CancelAfterCompletionDoesNothing)
{
    AgentRun run(fakeConfig(replayArgs(QStringLiteral("simple_answer.jsonl"))));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasResult);
    run.cancel();
    QCoreApplication::processEvents();
    EXPECT_FALSE(run.isRunning());
}

TEST(AgentRun, StartTwiceIsRejected)
{
    AgentRun run(fakeConfig(replayArgs(QStringLiteral("simple_answer.jsonl"))));
    const Outcome o = runAgent(run, QStringLiteral("x"));
    ASSERT_TRUE(o.hasResult);
    EXPECT_FALSE(run.start(QStringLiteral("again")));
}

TEST(AgentRun, TenMegabytesOfOutputDoesNotStall)
{
    AgentRun run(
        fakeConfig({QStringLiteral("--fake=big"), QStringLiteral("--fake-bytes=10485760")}));
    int texts = 0;
    QObject::connect(&run, &AgentRun::assistantText, [&texts](const QString&) { ++texts; });
    const Outcome o = runAgent(run, QStringLiteral("x"), 60000);
    ASSERT_TRUE(o.hasResult) << (o.hasError ? o.error.message.toStdString() : "timeout");
    EXPECT_GT(texts, 9000);
    EXPECT_EQ(run.unparsedLines(), 0);
}

TEST(AgentRun, TenMegabyteSingleLine)
{
    AgentRun run(
        fakeConfig({QStringLiteral("--fake=bigline"), QStringLiteral("--fake-bytes=10485760")}));
    const Outcome o = runAgent(run, QStringLiteral("x"), 60000);
    ASSERT_TRUE(o.hasResult) << (o.hasError ? o.error.message.toStdString() : "timeout");
    ASSERT_GE(o.log.size(), 2);
    EXPECT_EQ(o.log.at(1).size(), 5 + 10485760);
}

TEST(AgentRun, DestroyingWhileRunningKillsTheProcess)
{
    auto run = std::make_unique<AgentRun>(fakeConfig({QStringLiteral("--fake=hang")}));
    ASSERT_TRUE(run->start(QStringLiteral("x")));
    QElapsedTimer clock;
    clock.start();
    run.reset(); // must not hang or crash
    EXPECT_LT(clock.elapsed(), 5000);
}
