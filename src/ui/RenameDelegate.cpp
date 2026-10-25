#include "ui/RenameDelegate.h"

#include <QApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QStyle>
#include <QVBoxLayout>

#include "core/NoteFiles.h"
#include "ui/Icons.h"
#include "ui/ThemeManager.h"
#include "ui/VaultTreeModel.h"

namespace nexus::ui {

namespace {

constexpr int kRadius = 5;
constexpr int kArrowSize = 12;
constexpr int kFileIconSize = 13;
constexpr int kDotSize = 7;
constexpr int kDepthStep = 16;
constexpr int kRightPad = 8;
constexpr int kCountFontPx = 11;
constexpr qreal kFocusLine = 1.5;
constexpr int kEditorHeight = 28;

// The editor: the input plus, under it, the reason a name is refused.
class RenameEditor : public QWidget {
public:
    RenameEditor(QWidget* parent, QString rel, QString original)
        : QWidget(parent), m_rel(std::move(rel)), m_original(std::move(original)),
          m_input(new QLineEdit(this)), m_error(new QLabel(this))
    {
        setObjectName(QStringLiteral("renameEditor"));
        setAutoFillBackground(false);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 1, 0, 1);
        layout->setSpacing(4);
        layout->addWidget(m_input);
        m_error->setObjectName(QStringLiteral("renameError"));
        m_error->setWordWrap(true);
        m_error->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_error->hide();
        layout->addWidget(m_error);
        setFocusProxy(m_input);
    }

    [[nodiscard]] QLineEdit* input() const { return m_input; }
    [[nodiscard]] const QString& rel() const { return m_rel; }
    [[nodiscard]] bool valid() const { return m_valid; }
    [[nodiscard]] bool changed() const { return m_input->text() != m_original; }
    [[nodiscard]] bool closing() const { return m_closing; }
    void setClosing() { m_closing = true; }

    void showError(const std::optional<QString>& message)
    {
        m_valid = !message.has_value();
        m_input->setProperty("invalid", !m_valid);
        m_input->style()->unpolish(m_input);
        m_input->style()->polish(m_input);
        m_error->setText(message.value_or(QString()));
        m_error->setVisible(!m_valid);
        resize(width(), qMax(kEditorHeight, heightForWidth(width())));
    }

private:
    QString m_rel;
    QString m_original;
    QLineEdit* m_input;
    QLabel* m_error;
    bool m_valid = true;
    bool m_closing = false;
};

} // namespace

QString describeNameError(core::NameError error)
{
    using core::NameError;
    switch (error) {
    case NameError::Empty:
        return QObject::tr("名稱不能是空的");
    case NameError::InvalidCharacter:
        return QObject::tr("名稱不能包含 \\ / : * ? \" < > |");
    case NameError::ControlCharacter:
        return QObject::tr("名稱不能包含控制字元");
    case NameError::TrailingSpaceOrDot:
        return QObject::tr("名稱的結尾不能是空格或句點");
    case NameError::ReservedName:
        return QObject::tr("這是系統保留的名稱");
    case NameError::TooLong:
        return QObject::tr("名稱太長");
    case NameError::Hidden:
        return QObject::tr("名稱不能以句點開頭");
    }
    return {};
}

RenameDelegate::RenameDelegate(ThemeManager& theme, QObject* parent)
    : QStyledItemDelegate(parent), m_theme(theme)
{
}

int RenameDelegate::depthOf(const QModelIndex& index)
{
    int depth = 0;
    for (QModelIndex p = index.parent(); p.isValid(); p = p.parent()) {
        ++depth;
    }
    return depth;
}

int RenameDelegate::iconX(int depth, bool folder)
{
    // Main.dc.html: folders start at 8px; a note inside a folder at 28px, a note in the root
    // (no arrow column to align with) at 10px.
    if (folder) {
        return 8 + depth * kDepthStep;
    }
    return depth == 0 ? 10 : 12 + depth * kDepthStep;
}

QPixmap RenameDelegate::tinted(const QString& icon, const QColor& color, int size, qreal dpr) const
{
    const QString key = icon + QLatin1Char('|') + color.name(QColor::HexArgb) + QLatin1Char('|') +
                        QString::number(dpr);
    auto it = m_icons.find(key);
    if (it == m_icons.end()) {
        it = m_icons.insert(key, icons::pixmap(icon, color, size, dpr));
    }
    return it.value();
}

QSize RenameDelegate::sizeHint(const QStyleOptionViewItem& option,
                               const QModelIndex& /*index*/) const
{
    return {option.rect.width(), kRowHeight};
}

void RenameDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    const Palette& pal = m_theme.palette();
    const bool folder = index.data(VaultTreeModel::IsFolderRole).toBool();
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const bool hovered = (option.state & QStyle::State_MouseOver) != 0;
    const bool focused =
        (option.state & QStyle::State_HasFocus) != 0 && (option.state & QStyle::State_Active) != 0;
    const int depth = depthOf(index);
    const QRect row(option.rect.left(), option.rect.top(), option.rect.width(), kRowHeight);
    const qreal dpr = painter->device()->devicePixelRatioF();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    painter->setPen(Qt::NoPen);
    if (selected) {
        painter->setBrush(pal.accentSoft);
    } else if (hovered || focused) {
        painter->setBrush(pal.hover);
    } else {
        painter->setBrush(Qt::NoBrush);
    }
    painter->drawRoundedRect(row, kRadius, kRadius);
    if (focused) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(pal.accent, kFocusLine));
        const qreal inset = kFocusLine / 2;
        painter->drawRoundedRect(QRectF(row).adjusted(inset, inset, -inset, -inset), kRadius,
                                 kRadius);
    }

    const QColor textColor = selected ? pal.accentText : pal.text;
    QFont font = option.font;
    font.setWeight((folder || selected) ? QFont::Medium : QFont::Normal);
    painter->setFont(font);
    const QFontMetrics fm(font);

    int textLeft = 0;
    int textRight = row.right() - kRightPad;
    if (folder) {
        const bool open = (option.state & QStyle::State_Open) != 0;
        const int x = row.left() + iconX(depth, true);
        const QPixmap arrow =
            tinted(open ? QStringLiteral("chevron-down") : QStringLiteral("chevron-right"),
                   pal.muted, kArrowSize, dpr);
        painter->drawPixmap(x, row.top() + (kRowHeight - kArrowSize) / 2, arrow);
        textLeft = x + kArrowSize + 6;

        const int count = index.data(VaultTreeModel::NoteCountRole).toInt();
        if (!open && count > 0) {
            QFont small = option.font;
            small.setPixelSize(kCountFontPx);
            painter->setFont(small);
            painter->setPen(pal.muted);
            const QString label = QString::number(count);
            const int width = QFontMetrics(small).horizontalAdvance(label);
            painter->drawText(QRect(textRight - width, row.top(), width, kRowHeight),
                              Qt::AlignRight | Qt::AlignVCenter, label);
            textRight -= width + 6;
            painter->setFont(font);
        }
    } else {
        const int x = row.left() + iconX(depth, false);
        const QPixmap file = tinted(QStringLiteral("file"), selected ? pal.accentText : pal.muted,
                                    kFileIconSize, dpr);
        painter->drawPixmap(x, row.top() + (kRowHeight - kFileIconSize) / 2, file);
        textLeft = x + kFileIconSize + 8;

        if (index.data(VaultTreeModel::DirtyRole).toBool()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(pal.accent);
            painter->drawEllipse(QRect(textRight - kDotSize + 1,
                                       row.top() + (kRowHeight - kDotSize) / 2, kDotSize,
                                       kDotSize));
            textRight -= kDotSize + 8;
        }
    }

    painter->setPen(textColor);
    const QString text = fm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                                       qMax(0, textRight - textLeft + 1));
    painter->drawText(QRect(textLeft, row.top(), qMax(0, textRight - textLeft + 1), kRowHeight),
                      Qt::AlignLeft | Qt::AlignVCenter, text);
    painter->restore();
}

// ---- rename editor ------------------------------------------------------------------------

std::optional<QString> RenameDelegate::validate(const QModelIndex& index, const QString& newName)
{
    const QString current = index.data(Qt::EditRole).toString();
    if (newName == current) {
        return std::nullopt;
    }
    if (const auto bad = core::validateName(newName)) {
        return describeNameError(*bad);
    }
    const auto* model = qobject_cast<const VaultTreeModel*>(index.model());
    if (model != nullptr && model->siblingNameTaken(index, newName)) {
        return QObject::tr("此資料夾已有「%1」").arg(newName);
    }
    return std::nullopt;
}

QWidget* RenameDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& /*option*/,
                                      const QModelIndex& index) const
{
    auto* editor = new RenameEditor(parent, index.data(VaultTreeModel::RelPathRole).toString(),
                                    index.data(Qt::EditRole).toString());
    const QPersistentModelIndex persistent(index);
    connect(editor->input(), &QLineEdit::textChanged, editor,
            [editor, persistent](const QString& t) {
                if (persistent.isValid()) {
                    editor->showError(validate(persistent, t));
                }
            });
    editor->input()->installEventFilter(const_cast<RenameDelegate*>(this));
    return editor;
}

void RenameDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* rename = dynamic_cast<RenameEditor*>(editor);
    if (rename == nullptr) {
        return;
    }
    rename->input()->setText(index.data(Qt::EditRole).toString());
    rename->input()->selectAll();
}

void RenameDelegate::setModelData(QWidget* /*editor*/, QAbstractItemModel* /*model*/,
                                  const QModelIndex& /*index*/) const
{
    // The owner applies the new name once the file operation succeeded (renameRequested).
}

void RenameDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const
{
    const bool folder = index.data(VaultTreeModel::IsFolderRole).toBool();
    const int left = option.rect.left() + iconX(depthOf(index), folder) - 4;
    const int width = qMax(0, option.rect.right() - 4 - left);
    editor->setGeometry(left, option.rect.top(), width,
                        qMax(kEditorHeight, editor->heightForWidth(width)));
}

bool RenameDelegate::eventFilter(QObject* watched, QEvent* event)
{
    auto* input = qobject_cast<QLineEdit*>(watched);
    auto* editor = input != nullptr ? dynamic_cast<RenameEditor*>(input->parentWidget()) : nullptr;
    if (editor == nullptr) {
        return QStyledItemDelegate::eventFilter(watched, event);
    }
    const auto finish = [this, editor](bool confirm) {
        if (editor->closing()) {
            return;
        }
        editor->setClosing();
        if (confirm && editor->changed()) {
            emit renameRequested(editor->rel(), editor->input()->text());
        }
        emit closeEditor(editor, QAbstractItemDelegate::NoHint);
    };

    if (event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            if (editor->valid()) {
                finish(true);
            }
            return true; // an invalid name keeps the editor open
        }
        if (key == Qt::Key_Escape) {
            finish(false);
            return true;
        }
    } else if (event->type() == QEvent::FocusOut) {
        const auto reason = static_cast<QFocusEvent*>(event)->reason();
        if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason) {
            finish(editor->valid());
        }
    }
    return false;
}

} // namespace nexus::ui
