#include "ai/AgentRun.h"

#include <QFileInfo>
#include <QMetaType>
#include <QStandardPaths>

#include <utility>

namespace nexus::ai {

namespace {

constexpr qsizetype kStderrKeep = 16384;

// Resolves the configured CLI to an absolute executable path; empty if it does not exist.
QString resolveExecutable(const QString& configured)
{
    if (configured.contains(QLatin1Char('/')) || configured.contains(QLatin1Char('\\'))) {
        const QFileInfo info(configured);
        return info.isFile() ? info.absoluteFilePath() : QString();
    }
    return QStandardPaths::findExecutable(configured);
}

} // namespace

AgentRun::AgentRun(AgentRunConfig config, QObject* parent)
    : QObject(parent), m_config(std::move(config)), m_process(this), m_timeoutTimer(this),
      m_killTimer(this)
{
    qRegisterMetaType<nexus::ai::AgentResult>("nexus::ai::AgentResult");
    qRegisterMetaType<nexus::ai::AgentError>("nexus::ai::AgentError");

    m_timeoutTimer.setSingleShot(true);
    m_killTimer.setSingleShot(true);

    connect(&m_process, &QProcess::readyReadStandardOutput, this, &AgentRun::onReadyStdout);
    connect(&m_process, &QProcess::readyReadStandardError, this, &AgentRun::onReadyStderr);
    connect(&m_process, &QProcess::finished, this, &AgentRun::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred, this, &AgentRun::onProcessError);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this] {
        requestStop(AgentErrorKind::Timeout,
                    tr("The task timed out after %1 ms.").arg(m_config.timeoutMs));
    });
    connect(&m_killTimer, &QTimer::timeout, this, [this] { m_process.kill(); });
}

AgentRun::~AgentRun()
{
    // Never leave the CLI running behind a destroyed owner.
    if (m_process.state() != QProcess::NotRunning) {
        m_process.disconnect(this);
        m_process.kill();
        m_process.waitForFinished(2000);
    }
}

bool AgentRun::start(const QString& prompt)
{
    if (m_state != State::Idle) {
        return false;
    }
    m_state = State::Running;

    const QString exe = resolveExecutable(m_config.executable);
    if (exe.isEmpty()) {
        // Report asynchronously so callers can rely on connecting after start().
        QTimer::singleShot(0, this, [this] {
            fail(AgentError{AgentErrorKind::ExecutableNotFound,
                            tr("Claude Code CLI not found: %1").arg(m_config.executable),
                            {},
                            -1});
        });
        return true;
    }

    QStringList args{QStringLiteral("-p"), QStringLiteral("--output-format"),
                     QStringLiteral("stream-json"), QStringLiteral("--verbose")};
    args += m_config.extraArgs;
    if (!m_config.workingDirectory.isEmpty()) {
        m_process.setWorkingDirectory(m_config.workingDirectory);
    }
    m_process.start(exe, args);
    if (m_state == State::Done) {
        return true; // FailedToStart was already reported
    }
    // The prompt is queued now and flushed once the process is up; closing the write channel
    // afterwards signals end of input.
    m_process.write(prompt.toUtf8());
    m_process.closeWriteChannel();

    if (m_config.timeoutMs > 0) {
        m_timeoutTimer.start(m_config.timeoutMs);
    }
    emit started();
    return true;
}

void AgentRun::cancel()
{
    requestStop(AgentErrorKind::Cancelled, tr("The task was cancelled."));
}

void AgentRun::requestStop(AgentErrorKind reason, const QString& message)
{
    if (m_state != State::Running || m_stopReason.has_value()) {
        return;
    }
    if (m_process.state() == QProcess::NotRunning) {
        fail(AgentError{reason, message, stderrTail(), -1});
        return;
    }
    m_stopReason = AgentError{reason, message, {}, -1};
    m_timeoutTimer.stop();
    m_process.terminate();
    m_killTimer.start(m_config.killGraceMs);
}

void AgentRun::onReadyStdout()
{
    const QByteArray data = m_process.readAllStandardOutput();
    const QList<QByteArray> lines = m_splitter.feed(data);
    for (const QByteArray& line : lines) {
        handleLine(line);
    }
}

void AgentRun::onReadyStderr()
{
    m_stderr += m_process.readAllStandardError();
    if (m_stderr.size() > kStderrKeep * 2) {
        m_stderr = m_stderr.right(kStderrKeep);
    }
}

QString AgentRun::stderrTail() const
{
    return QString::fromUtf8(m_stderr.right(kStderrKeep)).trimmed();
}

void AgentRun::handleLine(const QByteArray& line)
{
    const auto parsed = parseStreamJsonLine(line);
    if (!parsed) {
        ++m_unparsedLines;
        if (m_firstParseProblem.isEmpty()) {
            m_firstParseProblem = parsed.error().message + QStringLiteral(": ") +
                                  QString::fromUtf8(parsed.error().line);
        }
        return;
    }
    if (!parsed->ignoredType.isEmpty() && !m_ignoredTypes.contains(parsed->ignoredType)) {
        m_ignoredTypes.append(parsed->ignoredType);
    }
    for (const AgentEvent& event : parsed->events) {
        handleEvent(event);
    }
}

void AgentRun::handleEvent(const AgentEvent& event)
{
    if (const auto* init = std::get_if<SessionInit>(&event)) {
        emit sessionInitialized(init->sessionId, init->model);
    } else if (const auto* text = std::get_if<AssistantText>(&event)) {
        emit assistantText(text->text);
    } else if (const auto* use = std::get_if<ToolUse>(&event)) {
        emit toolUse(use->name, use->inputSummary);
    } else if (const auto* res = std::get_if<ToolResult>(&event)) {
        emit toolResult(res->isError, res->summary);
    } else if (const auto* result = std::get_if<AgentResult>(&event)) {
        // Held back until the process has ended so finished() is the last signal.
        m_result = *result;
    }
}

void AgentRun::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        fail(AgentError{AgentErrorKind::StartFailed,
                        tr("Could not start the CLI: %1").arg(m_process.errorString()),
                        stderrTail(), -1});
    }
    // Crashed etc. are reported through finished().
}

void AgentRun::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_timeoutTimer.stop();
    m_killTimer.stop();
    if (m_state != State::Running) {
        return;
    }
    onReadyStdout();
    onReadyStderr();
    if (const auto rest = m_splitter.finish()) {
        handleLine(*rest);
    }
    if (m_splitter.overflowed() > 0) {
        m_unparsedLines += m_splitter.overflowed();
        if (m_firstParseProblem.isEmpty()) {
            m_firstParseProblem = tr("A line exceeded the maximum length.");
        }
    }

    if (m_stopReason) {
        AgentError error = *m_stopReason;
        error.stderrText = stderrTail();
        fail(error);
    } else if (m_result) {
        succeed(*m_result);
    } else if (status == QProcess::CrashExit || exitCode != 0) {
        fail(AgentError{AgentErrorKind::NonZeroExit,
                        status == QProcess::CrashExit
                            ? tr("The CLI crashed.")
                            : tr("The CLI exited with code %1.").arg(exitCode),
                        stderrTail(), exitCode});
    } else {
        fail(AgentError{AgentErrorKind::ParseError,
                        m_firstParseProblem.isEmpty()
                            ? tr("The CLI output ended without a result.")
                            : tr("Could not parse the CLI output: %1").arg(m_firstParseProblem),
                        stderrTail(), exitCode});
    }
}

void AgentRun::fail(const AgentError& error)
{
    if (m_state == State::Done) {
        return;
    }
    m_state = State::Done;
    m_timeoutTimer.stop();
    m_killTimer.stop();
    emit failed(error);
}

void AgentRun::succeed(const AgentResult& result)
{
    if (m_state == State::Done) {
        return;
    }
    m_state = State::Done;
    emit finished(result);
}

} // namespace nexus::ai
