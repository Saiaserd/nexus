#include "ui/VaultStatusWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>

#include "ui/ThemeManager.h"

namespace nexus::ui {

class VaultStatusWidget::Dot : public QWidget {
public:
    Dot(ThemeManager& theme, QWidget* parent) : QWidget(parent), m_theme(theme)
    {
        setFixedSize(7, 7);
        connect(&m_theme, &ThemeManager::themeChanged, this, qOverload<>(&QWidget::update));
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_theme.palette().ok);
        painter.drawEllipse(rect());
    }

private:
    ThemeManager& m_theme;
};

VaultStatusWidget::VaultStatusWidget(ThemeManager& theme, QWidget* parent)
    : QWidget(parent), m_watching(new QWidget(this)), m_idle(new QLabel(this)),
      m_count(new QLabel(this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 0, 0); // design: footer padding 0 12px
    layout->setSpacing(18);

    layout->addWidget(m_idle);

    auto* watchLayout = new QHBoxLayout(m_watching);
    watchLayout->setContentsMargins(0, 0, 0, 0);
    watchLayout->setSpacing(6);
    watchLayout->addWidget(new Dot(theme, m_watching), 0, Qt::AlignVCenter);
    watchLayout->addWidget(new QLabel(tr("監看中"), m_watching));
    layout->addWidget(m_watching);

    layout->addWidget(m_count);

    setState(VaultSession::State::Closed, 0);
}

void VaultStatusWidget::setState(VaultSession::State state, int noteCount)
{
    const bool open = state == VaultSession::State::Open;
    m_watching->setVisible(open);
    m_count->setVisible(open);
    m_idle->setVisible(!open);
    m_idle->setText(state == VaultSession::State::Opening ? tr("正在開啟 vault…")
                                                          : tr("尚未開啟 vault"));
    m_count->setText(tr("%1 篇筆記").arg(noteCount));
}

QString VaultStatusWidget::summary() const
{
    if (m_idle->isVisibleTo(this)) {
        return m_idle->text();
    }
    return tr("監看中 %1").arg(m_count->text());
}

} // namespace nexus::ui
