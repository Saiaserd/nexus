#include "ai/StreamJsonParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>

#include <utility>

namespace nexus::ai {

namespace {

constexpr qsizetype kSummaryMax = 200;
constexpr qsizetype kErrorLineMax = 200;
// Real events nest a dozen levels at most. Qt's JSON parser overflows the stack on
// pathological nesting (seen in a debug build), so deep input is rejected up front.
constexpr int kMaxJsonDepth = 128;

bool exceedsJsonDepth(const QByteArray& text)
{
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (const char c : text) {
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
        } else if (c == '"') {
            inString = true;
        } else if (c == '[' || c == '{') {
            if (++depth > kMaxJsonDepth) {
                return true;
            }
        } else if ((c == ']' || c == '}') && depth > 0) {
            --depth;
        }
    }
    return false;
}

QString truncated(QString text)
{
    text = text.simplified();
    if (text.size() > kSummaryMax) {
        text.truncate(kSummaryMax);
        text += QChar(0x2026);
    }
    return text;
}

QJsonArray messageContent(const QJsonObject& root)
{
    return root.value(QLatin1String("message")).toObject().value(QLatin1String("content")).toArray();
}

// Short digest of a tool_use input object: the most telling single field, else compact JSON.
QString summarizeToolInput(const QJsonValue& input)
{
    if (!input.isObject()) {
        return {};
    }
    const QJsonObject obj = input.toObject();
    for (const char* key : {"command", "file_path", "path", "pattern", "url", "query", "prompt",
                            "description"}) {
        const QJsonValue v = obj.value(QLatin1String(key));
        if (v.isString() && !v.toString().isEmpty()) {
            return truncated(v.toString());
        }
    }
    return truncated(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

// tool_result.content is either a plain string or an array of {type:"text", text:...} blocks.
QString summarizeToolResult(const QJsonValue& content)
{
    if (content.isString()) {
        return truncated(content.toString());
    }
    if (content.isArray()) {
        QStringList parts;
        const QJsonArray blocks = content.toArray();
        for (const auto& block : blocks) {
            const QJsonObject o = block.toObject();
            if (o.value(QLatin1String("type")).toString() == QLatin1String("text")) {
                parts.append(o.value(QLatin1String("text")).toString());
            }
        }
        return truncated(parts.join(QLatin1Char(' ')));
    }
    return {};
}

void parseAssistant(const QJsonObject& root, QList<AgentEvent>& out)
{
    const QJsonArray content = messageContent(root);
    for (const auto& blockValue : content) {
        const QJsonObject block = blockValue.toObject();
        const QString type = block.value(QLatin1String("type")).toString();
        if (type == QLatin1String("text")) {
            out.append(AssistantText{block.value(QLatin1String("text")).toString()});
        } else if (type == QLatin1String("tool_use")) {
            out.append(ToolUse{block.value(QLatin1String("id")).toString(),
                               block.value(QLatin1String("name")).toString(),
                               summarizeToolInput(block.value(QLatin1String("input")))});
        }
        // "thinking" and any future block type: skipped on purpose.
    }
}

void parseUser(const QJsonObject& root, QList<AgentEvent>& out)
{
    // For plain user messages `content` is a string; only tool_result arrays matter.
    const QJsonArray content = messageContent(root);
    for (const auto& blockValue : content) {
        const QJsonObject block = blockValue.toObject();
        if (block.value(QLatin1String("type")).toString() == QLatin1String("tool_result")) {
            out.append(ToolResult{block.value(QLatin1String("tool_use_id")).toString(),
                                  block.value(QLatin1String("is_error")).toBool(false),
                                  summarizeToolResult(block.value(QLatin1String("content")))});
        }
    }
}

AgentResult parseResult(const QJsonObject& root)
{
    AgentResult r;
    const QString subtype = root.value(QLatin1String("subtype")).toString();
    r.success = subtype == QLatin1String("success") &&
                !root.value(QLatin1String("is_error")).toBool(false);
    const QJsonValue duration = root.contains(QLatin1String("duration_ms"))
                                    ? root.value(QLatin1String("duration_ms"))
                                    : root.value(QLatin1String("duration_api_ms"));
    r.durationMs = static_cast<qint64>(duration.toDouble(0));
    for (const char* key : {"total_cost_usd", "cost_usd"}) {
        const QJsonValue cost = root.value(QLatin1String(key));
        if (cost.isDouble()) {
            r.costUsd = cost.toDouble();
            break;
        }
    }
    r.sessionId = root.value(QLatin1String("session_id")).toString();
    const QJsonValue text = root.value(QLatin1String("result"));
    if (text.isString()) {
        r.finalText = text.toString();
    } else if (root.value(QLatin1String("errors")).isArray()) {
        QStringList errors;
        const QJsonArray arr = root.value(QLatin1String("errors")).toArray();
        for (const auto& e : arr) {
            errors.append(e.toString());
        }
        r.finalText = errors.join(QLatin1String("; "));
    }
    if (r.finalText.isEmpty() && !r.success) {
        r.finalText = subtype;
    }
    return r;
}

} // namespace

Expected<ParsedLine, ParseError> parseStreamJsonLine(const QByteArray& line)
{
    if (exceedsJsonDepth(line)) {
        return unexpected(
            ParseError{QStringLiteral("JSON nesting too deep"), line.left(kErrorLineMax)});
    }
    QJsonParseError jsonError;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &jsonError);
    if (jsonError.error != QJsonParseError::NoError) {
        return unexpected(ParseError{jsonError.errorString(), line.left(kErrorLineMax)});
    }
    if (!doc.isObject()) {
        return unexpected(
            ParseError{QStringLiteral("JSON value is not an object"), line.left(kErrorLineMax)});
    }
    const QJsonObject root = doc.object();
    const QString type = root.value(QLatin1String("type")).toString();

    ParsedLine parsed;
    if (type == QLatin1String("system")) {
        if (root.value(QLatin1String("subtype")).toString() == QLatin1String("init")) {
            parsed.events.append(SessionInit{root.value(QLatin1String("session_id")).toString(),
                                             root.value(QLatin1String("model")).toString()});
        } else {
            parsed.ignoredType =
                QStringLiteral("system/") + root.value(QLatin1String("subtype")).toString();
        }
    } else if (type == QLatin1String("assistant")) {
        parseAssistant(root, parsed.events);
    } else if (type == QLatin1String("user")) {
        parseUser(root, parsed.events);
    } else if (type == QLatin1String("result")) {
        parsed.events.append(parseResult(root));
    } else {
        parsed.ignoredType = type.isEmpty() ? QStringLiteral("(no type)") : type;
    }
    return parsed;
}

LineSplitter::LineSplitter(std::size_t maxLineBytes) : m_maxLine(maxLineBytes) {}

QList<QByteArray> LineSplitter::feed(const QByteArray& chunk)
{
    QList<QByteArray> lines;
    m_buffer.append(chunk);

    qsizetype start = 0;
    qsizetype from = m_scanned;
    for (;;) {
        const qsizetype nl = m_buffer.indexOf('\n', from);
        if (nl < 0) {
            break;
        }
        if (m_discarding) {
            m_discarding = false;
        } else {
            QByteArray line = m_buffer.mid(start, nl - start);
            if (line.endsWith('\r')) {
                line.chop(1);
            }
            if (static_cast<std::size_t>(line.size()) > m_maxLine) {
                ++m_overflowed;
            } else if (!line.isEmpty()) {
                lines.append(std::move(line));
            }
        }
        start = nl + 1;
        from = start;
    }
    m_buffer.remove(0, start);

    if (m_discarding) {
        m_buffer.clear();
    } else if (static_cast<std::size_t>(m_buffer.size()) > m_maxLine) {
        ++m_overflowed;
        m_discarding = true;
        m_buffer.clear();
    }
    m_scanned = m_buffer.size();
    return lines;
}

std::optional<QByteArray> LineSplitter::finish()
{
    QByteArray rest = std::exchange(m_buffer, QByteArray());
    m_scanned = 0;
    if (rest.endsWith('\r')) {
        rest.chop(1);
    }
    if (m_discarding || rest.isEmpty()) {
        return std::nullopt;
    }
    return rest;
}

} // namespace nexus::ai
