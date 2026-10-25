#pragma once

#include <cstdint>
#include <optional>

#include <QByteArray>
#include <QString>

namespace nexus::core {

enum class LineEnding : std::uint8_t { Lf, Crlf };

// The content of a note as the editor sees it: text always uses '\n'. The original line
// ending and BOM are remembered so saving does not silently reformat the user's file
// (ADR 0004). Files that mix LF and CRLF are normalized to the majority kind on write.
struct NoteDocument {
    QString text;
    LineEnding lineEnding = LineEnding::Lf;
    bool hasBom = false;

    friend bool operator==(const NoteDocument& a, const NoteDocument& b)
    {
        return a.text == b.text && a.lineEnding == b.lineEnding && a.hasBom == b.hasBom;
    }

    // nullopt when the bytes are not valid UTF-8.
    [[nodiscard]] static std::optional<NoteDocument> decode(const QByteArray& bytes);
    [[nodiscard]] QByteArray encode() const;
};

} // namespace nexus::core
