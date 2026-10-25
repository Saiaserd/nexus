#include "ui/ElidedLabel.h"

#include <QFontMetrics>
#include <QPainter>
#include <QStyleOption>

namespace nexus::ui {

ElidedLabel::ElidedLabel(QWidget* parent) : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

ElidedLabel::ElidedLabel(const QString& text, QWidget* parent) : ElidedLabel(parent)
{
    setFullText(text);
}

void ElidedLabel::setFullText(const QString& text)
{
    m_text = text;
    setToolTip(text);
    updateGeometry();
    update();
}

QString ElidedLabel::elidedText() const
{
    return fontMetrics().elidedText(m_text, Qt::ElideRight, contentsRect().width());
}

QSize ElidedLabel::sizeHint() const
{
    return {fontMetrics().horizontalAdvance(m_text), fontMetrics().height()};
}

QSize ElidedLabel::minimumSizeHint() const
{
    return {0, fontMetrics().height()};
}

void ElidedLabel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    // Honour style sheet box model (padding / background) before drawing the text.
    QStyleOption option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);

    painter.setPen(palette().color(foregroundRole()));
    painter.setFont(font());
    painter.drawText(contentsRect(), Qt::AlignVCenter | Qt::AlignLeft, elidedText());
}

} // namespace nexus::ui
