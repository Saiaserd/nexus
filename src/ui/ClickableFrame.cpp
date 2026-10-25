#include "ui/ClickableFrame.h"

#include <QKeyEvent>
#include <QMouseEvent>

namespace nexus::ui {

ClickableFrame::ClickableFrame(QWidget* parent) : QFrame(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
}

void ClickableFrame::mousePressEvent(QMouseEvent* event)
{
    m_pressed = event->button() == Qt::LeftButton;
    QFrame::mousePressEvent(event);
}

void ClickableFrame::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_pressed && event->button() == Qt::LeftButton &&
        rect().contains(event->position().toPoint())) {
        m_pressed = false;
        emit clicked();
        return;
    }
    m_pressed = false;
    QFrame::mouseReleaseEvent(event);
}

void ClickableFrame::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        emit clicked();
        return;
    default:
        break;
    }
    QFrame::keyPressEvent(event);
}

} // namespace nexus::ui
