#pragma once

#include <QFrame>

class QLabel;

namespace nexus::ui {

class ElidedLabel;
class ThemeManager;

// Left column of the workspace: header (folder icon and vault name, 40px) and the body that the
// file tree (T-112) will fill. Until then the body only summarises the vault.
class VaultSidebar : public QFrame {
    Q_OBJECT

public:
    explicit VaultSidebar(ThemeManager& theme, QWidget* parent = nullptr);

    void setVault(const QString& name, int noteCount);
    void setNoteCount(int count);
    [[nodiscard]] QString vaultName() const;

private:
    void applyIcons();

    ThemeManager& m_theme;
    QLabel* m_icon = nullptr;
    ElidedLabel* m_name = nullptr;
    QLabel* m_summary;
};

} // namespace nexus::ui
