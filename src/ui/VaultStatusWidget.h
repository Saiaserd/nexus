#pragma once

#include <QWidget>

#include "ui/VaultSession.h"

class QLabel;

namespace nexus::ui {

class ThemeManager;

// Left part of the status bar: "尚未開啟 vault", "正在開啟 vault…" or the green dot with
// "監看中" and the note count (design: 7px dot, 6px gap, 18px between items).
class VaultStatusWidget : public QWidget {
    Q_OBJECT

public:
    explicit VaultStatusWidget(ThemeManager& theme, QWidget* parent = nullptr);

    void setState(VaultSession::State state, int noteCount);
    [[nodiscard]] QString summary() const;

private:
    class Dot;

    QWidget* m_watching;
    QLabel* m_idle;
    QLabel* m_count;
};

} // namespace nexus::ui
