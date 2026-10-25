#pragma once

#include <QFrame>

namespace nexus::ui {

// A frame that behaves like a button: hover and focus states come from the style sheet, it
// takes keyboard focus and activates on click, Enter and Space. Used for the welcome cards
// and the recent-vault rows, which hold several labels (a QPushButton cannot).
class ClickableFrame : public QFrame {
    Q_OBJECT

public:
    explicit ClickableFrame(QWidget* parent = nullptr);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    bool m_pressed = false;
};

} // namespace nexus::ui
