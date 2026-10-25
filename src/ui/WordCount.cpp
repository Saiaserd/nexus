#include "ui/WordCount.h"

#include <QChar>

namespace nexus::ui {

namespace {

bool isCjk(char32_t ucs4)
{
    switch (QChar::script(ucs4)) {
    case QChar::Script_Han:
    case QChar::Script_Hiragana:
    case QChar::Script_Katakana:
    case QChar::Script_Hangul:
    case QChar::Script_Bopomofo:
        return true;
    default:
        return false;
    }
}

bool isWordChar(char32_t ucs4)
{
    return QChar::isLetterOrNumber(ucs4) || QChar::category(ucs4) == QChar::Mark_NonSpacing;
}

} // namespace

int countWords(QStringView text)
{
    int count = 0;
    bool inWord = false;
    const qsizetype size = text.size();
    for (qsizetype i = 0; i < size; ++i) {
        char32_t ucs4 = text[i].unicode();
        if (QChar::isHighSurrogate(ucs4) && i + 1 < size && text[i + 1].isLowSurrogate()) {
            ucs4 = QChar::surrogateToUcs4(text[i].unicode(), text[i + 1].unicode());
            ++i;
        }
        if (isCjk(ucs4)) {
            ++count;
            inWord = false;
        } else if (isWordChar(ucs4)) {
            if (!inWord) {
                ++count;
                inWord = true;
            }
        } else {
            inWord = false;
        }
    }
    return count;
}

} // namespace nexus::ui
