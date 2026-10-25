#include "core/NoteDocument.h"

#include <QByteArrayView>

namespace nexus::core {

namespace {

const QByteArray& bomBytes()
{
    static const QByteArray bom = QByteArray::fromHex("EFBBBF");
    return bom;
}

} // namespace

std::optional<NoteDocument> NoteDocument::decode(const QByteArray& bytes)
{
    NoteDocument doc;
    QByteArrayView body(bytes);
    if (body.startsWith(QByteArrayView(bomBytes()))) {
        doc.hasBom = true;
        body = body.sliced(bomBytes().size());
    }
    if (!body.isValidUtf8()) {
        return std::nullopt;
    }

    qsizetype crlf = 0;
    qsizetype lf = 0;
    for (qsizetype i = 0; i < body.size(); ++i) {
        if (body[i] == '\n') {
            if (i > 0 && body[i - 1] == '\r') {
                ++crlf;
            } else {
                ++lf;
            }
        }
    }
    doc.lineEnding = crlf > lf ? LineEnding::Crlf : LineEnding::Lf;

    QByteArray normalized = body.toByteArray();
    if (crlf > 0) {
        normalized.replace("\r\n", "\n");
    }
    doc.text = QString::fromUtf8(normalized);
    return doc;
}

QByteArray NoteDocument::encode() const
{
    QByteArray body = text.toUtf8();
    if (lineEnding == LineEnding::Crlf) {
        body.replace("\n", "\r\n");
    }
    if (hasBom) {
        body.prepend(bomBytes());
    }
    return body;
}

} // namespace nexus::core
