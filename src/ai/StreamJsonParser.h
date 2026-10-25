#pragma once

#include "core/Expected.h"

#include "ai/AgentEvent.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

#include <cstddef>
#include <optional>

namespace nexus::ai {

struct ParseError {
    QString message;
    QByteArray line; // offending line (truncated)
};

// Result of parsing one line. `events` may be empty: the line was valid JSON but of a kind we
// do not (yet) understand; `ignoredType` then names it so callers can log it.
struct ParsedLine {
    QList<AgentEvent> events;
    QString ignoredType;
};

// Pure, stateless parsing of one stream-json line (no trailing newline). Tolerant of unknown
// event types and unknown fields so a CLI upgrade never breaks the bridge; only text that is
// not a JSON object is an error.
Expected<ParsedLine, ParseError> parseStreamJsonLine(const QByteArray& line);

// Reassembles lines from arbitrarily sized chunks (a line may arrive split over several
// reads, or many lines in one). Handles both "\n" and "\r\n".
class LineSplitter {
public:
    // maxLineBytes: a line longer than this is dropped and reported through overflowed().
    explicit LineSplitter(std::size_t maxLineBytes = kDefaultMaxLine);

    // Appends data and returns the complete lines (without terminators, empty lines skipped).
    QList<QByteArray> feed(const QByteArray& chunk);
    // Returns the trailing unterminated line, if any (call once at end of stream).
    std::optional<QByteArray> finish();
    // Number of lines dropped for exceeding the limit.
    [[nodiscard]] int overflowed() const { return m_overflowed; }

    static constexpr std::size_t kDefaultMaxLine = std::size_t{64} * 1024 * 1024;

private:
    QByteArray m_buffer;
    std::size_t m_maxLine;
    qsizetype m_scanned = 0;
    bool m_discarding = false;
    int m_overflowed = 0;
};

} // namespace nexus::ai
