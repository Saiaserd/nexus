#include "ui/VaultSidebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/ElidedLabel.h"
#include "ui/Icons.h"
#include "ui/ThemeManager.h"

namespace nexus::ui {

VaultSidebar::VaultSidebar(ThemeManager& theme, QWidget* parent)
    : QFrame(parent), m_theme(theme), m_summary(new QLabel(this))
{
    setObjectName(QStringLiteral("sidebar"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("sidebarHeader"));
    header->setFixedHeight(40);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 6, 0);
    headerLayout->setSpacing(6);
    m_icon = new QLabel(header);
    m_icon->setFixedSize(14, 14);
    headerLayout->addWidget(m_icon);
    m_name = new ElidedLabel(header);
    m_name->setObjectName(QStringLiteral("sidebarTitle"));
    headerLayout->addWidget(m_name, 1);
    layout->addWidget(header);

    m_summary->setObjectName(QStringLiteral("sidebarSummary"));
    m_summary->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    layout->addWidget(m_summary, 1);

    connect(&m_theme, &ThemeManager::themeChanged, this, &VaultSidebar::applyIcons);
    applyIcons();
}

void VaultSidebar::applyIcons()
{
    m_icon->setPixmap(icons::pixmap(QStringLiteral("folder"), m_theme.palette().accent, 14,
                                    m_icon->devicePixelRatioF()));
}

void VaultSidebar::setVault(const QString& name, int noteCount)
{
    m_name->setFullText(name);
    setNoteCount(noteCount);
}

void VaultSidebar::setNoteCount(int count)
{
    m_summary->setText(tr("%1 篇筆記").arg(count));
}

QString VaultSidebar::vaultName() const
{
    return m_name->fullText();
}

} // namespace nexus::ui
