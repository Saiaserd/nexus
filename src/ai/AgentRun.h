#pragma once

#include "ai/AgentEvent.h"
#include "ai/StreamJsonParser.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>
#include <optional>

namespace nexus::ai {

struct AgentRunConfig {
    // CLI to run: a bare name is looked up on PATH (default "claude"), otherwise a path.
    QString executable = QStringLiteral("claude");
    // Appended after the fixed `-p --output-format stream-json --verbose`.
    QStringList extraArgs;
    // Working directory of the CLI (the project code folder). Empty = inherit.
    QString workingDirectory;
    // Overall timeout in milliseconds; 0 = none.
    int timeoutMs = 0;
    // After terminate() the process is killed if still alive after this many ms.
    int killGraceMs = 3000;
};

// Runs one Claude Code CLI task and turns its stream-json stdout into signals (T-401).
// Single use: construct, connect, start(prompt). Exactly one of finished() / failed() is
// emitted, after the process has ended. Asynchronous only (no blocking calls), so it is safe
// to use from the UI thread.
class AgentRun : public QObject {
    Q_OBJECT

public:
    explicit AgentRun(AgentRunConfig config, QObject* parent = nullptr);
    ~AgentRun() override;

    AgentRun(const AgentRun&) = delete;
    AgentRun& operator=(const AgentRun&) = delete;
    AgentRun(AgentRun&&) = delete;
    AgentRun& operator=(AgentRun&&) = delete;

    // Starts the CLI and feeds `prompt` through stdin. Returns false if already started.
    bool start(const QString& prompt);
    // Asks the process to stop (terminate, then kill after killGraceMs). Ends with
    // failed(Cancelled). No effect once the run is over.
    void cancel();

    [[nodiscard]] bool isRunning() const { return m_state == State::Running; }
    // Lines of stdout that were not understood (malformed JSON / oversized).
    [[nodiscard]] int unparsedLines() const { return m_unparsedLines; }
    // Unknown (valid JSON, unsupported) event types seen so far, for diagnostics.
    [[nodiscard]] const QStringList& ignoredTypes() const { return m_ignoredTypes; }

signals:
    void started();
    void sessionInitialized(const QString& sessionId, const QString& model);
    void assistantText(const QString& text);
    void toolUse(const QString& name, const QString& inputSummary);
    void toolResult(bool isError, const QString& summary);
    void finished(const nexus::ai::AgentResult& result);
    void failed(const nexus::ai::AgentError& error);

private:
    enum class State : std::uint8_t { Idle, Running, Done };

    void onReadyStdout();
    void onReadyStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void handleLine(const QByteArray& line);
    void handleEvent(const AgentEvent& event);
    void requestStop(AgentErrorKind reason, const QString& message);
    void fail(const AgentError& error);
    void succeed(const AgentResult& result);
    [[nodiscard]] QString stderrTail() const;

    AgentRunConfig m_config;
    QProcess m_process;
    QTimer m_timeoutTimer;
    QTimer m_killTimer;
    LineSplitter m_splitter;
    State m_state = State::Idle;
    QByteArray m_stderr;
    std::optional<AgentResult> m_result;
    std::optional<AgentError> m_stopReason;
    QString m_firstParseProblem;
    QStringList m_ignoredTypes;
    int m_unparsedLines = 0;
};

} // namespace nexus::ai
