#pragma once

#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <optional>
#include <variant>

// Structured events produced from the Claude Code CLI `--output-format stream-json` stream
// (T-401). Plain value types; no Qt Widgets, no I/O.
namespace nexus::ai {

// `system/init` line: the session is up.
struct SessionInit {
    QString sessionId;
    QString model;
};

// A `text` block of an assistant message.
struct AssistantText {
    QString text;
};

// A `tool_use` block of an assistant message. inputSummary is a short human readable digest
// of the tool input (never the full payload).
struct ToolUse {
    QString toolUseId;
    QString name;
    QString inputSummary;
};

// A `tool_result` block of a user message.
struct ToolResult {
    QString toolUseId;
    bool isError = false;
    QString summary;
};

// Final `result` line.
struct AgentResult {
    bool success = false;
    qint64 durationMs = 0;
    std::optional<double> costUsd;
    QString sessionId;
    QString finalText; // `result` text on success, error description otherwise
};

using AgentEvent = std::variant<SessionInit, AssistantText, ToolUse, ToolResult, AgentResult>;

enum class AgentErrorKind : std::uint8_t {
    ExecutableNotFound, // CLI path does not resolve to an executable
    StartFailed,        // the process could not be started (permissions, bad binary, ...)
    NonZeroExit,        // exited non-zero (or crashed) without delivering a `result` line
    ParseError,         // stdout could not be understood (no usable result line)
    Timeout,            // overall timeout elapsed
    Cancelled           // cancel() was called
};

struct AgentError {
    AgentErrorKind kind = AgentErrorKind::StartFailed;
    QString message;
    QString stderrText; // collected stderr (tail), may be empty
    int exitCode = -1;  // valid for NonZeroExit
};

} // namespace nexus::ai
