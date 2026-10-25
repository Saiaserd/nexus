#include "ui/LoadingPage.h"

#include <QHideEvent>
#include <QLabel>
#include <QPainter>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace nexus::ui {

// Indeterminate progress: a segment sliding back and forth over a track. Drawn by hand because
// the style sheet disables the style's own indeterminate animation. Runs only while visible.
class BusyBar : public QWidget {
public:
    explicit BusyBar(QWidget* parent) : QWidget(parent)
    {
        setFixedSize(240, 4);
        m_animation.setStartValue(0.0);
        m_animation.setEndValue(1.0);
        m_animation.setDuration(1100);
        m_animation.setLoopCount(-1);
        m_animation.setEasingCurve(QEasingCurve::InOutSine);
        connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_position = v.toDouble();
            update();
        });
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        m_animation.start();
    }

    void hideEvent(QHideEvent* event) override
    {
        QWidget::hideEvent(event);
        m_animation.stop();
    }

    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        const qreal radius = height() / 2.0;
        painter.setBrush(palette().color(QPalette::Mid)); // palette Mid = theme border colour
        painter.drawRoundedRect(rect(), radius, radius);

        const qreal segment = width() * 0.35;
        const qreal x = (width() - segment) * m_position;
        painter.setBrush(palette().color(QPalette::Link)); // palette Link = theme accent link
        painter.drawRoundedRect(QRectF(x, 0, segment, height()), radius, radius);
    }

private:
    QVariantAnimation m_animation;
    qreal m_position = 0.0;
};

LoadingPage::LoadingPage(QWidget* parent)
    : QWidget(parent), m_message(new QLabel(this)), m_bar(new BusyBar(this))
{
    setObjectName(QStringLiteral("loadingPage"));
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(14);

    m_message->setObjectName(QStringLiteral("loadingMessage"));
    m_message->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_message, 0, Qt::AlignHCenter);

    layout->addWidget(m_bar, 0, Qt::AlignHCenter);
}

void LoadingPage::setVaultName(const QString& name)
{
    m_message->setText(tr("正在開啟「%1」…").arg(name));
}

QString LoadingPage::message() const
{
    return m_message->text();
}

} // namespace nexus::ui
