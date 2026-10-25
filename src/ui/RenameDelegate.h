#pragma once

#include <optional>

#include <QHash>
#include <QPixmap>
#include <QStyledItemDelegate>

#include "core/FileError.h"

namespace nexus::ui {

class ThemeManager;

// User-facing text for a rejected file or folder name (live validation, States.dc.html 2).
[[nodiscard]] QString describeNameError(core::NameError error);

// Item delegate of the file tree. It paints the rows (Main.dc.html left column: folder arrow,
// file icon, selection, unsaved dot, note count of a collapsed folder) and provides the
// in-place rename editor: Enter confirms, Esc cancels, an invalid name shows its reason under
// the input and cannot be confirmed. Confirming does not touch the model; the delegate only
// reports renameRequested() and the owner decides when the new name is applied.
class RenameDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    static constexpr int kRowHeight = 28;

    RenameDelegate(ThemeManager& theme, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

    // Why newName cannot be used for the item at index; nullopt when it is fine.
    [[nodiscard]] static std::optional<QString> validate(const QModelIndex& index,
                                                         const QString& newName);

signals:
    void renameRequested(const QString& relPath, const QString& newName);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    [[nodiscard]] QPixmap tinted(const QString& icon, const QColor& color, int size,
                                 qreal dpr) const;
    [[nodiscard]] static int depthOf(const QModelIndex& index);
    [[nodiscard]] static int iconX(int depth, bool folder);

    ThemeManager& m_theme;
    mutable QHash<QString, QPixmap> m_icons;
};

} // namespace nexus::ui
