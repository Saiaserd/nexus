#include "ui/VaultSidebar.h"

#include <QVBoxLayout>

#include "ui/FileTreePanel.h"

namespace nexus::ui {

VaultSidebar::VaultSidebar(ThemeManager& theme, QSettings* settings, QWidget* parent)
    : QFrame(parent), m_panel(new FileTreePanel(theme, this))
{
    setObjectName(QStringLiteral("sidebar"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_panel);

    m_panel->setSettings(settings);
    connect(m_panel, &FileTreePanel::openNoteRequested, this, &VaultSidebar::openNoteRequested);
    connect(m_panel, &FileTreePanel::noteRenamed, this, &VaultSidebar::noteRenamed);
    connect(m_panel, &FileTreePanel::noteRemoved, this, &VaultSidebar::noteRemoved);
}

void VaultSidebar::setSession(VaultSession& session)
{
    m_panel->setSession(session);
}

void VaultSidebar::setVault(const QString& name, int /*noteCount*/)
{
    m_panel->setVaultName(name);
}

void VaultSidebar::setNoteCount(int /*count*/) {}

QString VaultSidebar::vaultName() const
{
    return m_panel->vaultName();
}

void VaultSidebar::setNoteDirty(const QString& relPath, bool dirty)
{
    m_panel->setNoteDirty(relPath, dirty);
}

void VaultSidebar::selectNote(const QString& relPath)
{
    m_panel->selectNote(relPath);
}

void VaultSidebar::newNote()
{
    m_panel->newNote();
}

} // namespace nexus::ui
