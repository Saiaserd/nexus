#pragma once

#include <QStringView>

namespace nexus::ui {

// Word count shown in the status bar: every CJK character (Han, kana, Hangul) counts as one
// word; every other run of letters and digits counts as one word. Punctuation, symbols and
// whitespace are not counted.
int countWords(QStringView text);

} // namespace nexus::ui
