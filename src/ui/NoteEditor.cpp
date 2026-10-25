#include "ui/NoteEditor.h"

#include <algorithm>

#include <QEvent>
#include <QPainter>
#include <QTextBlock>

#include "ui/Fonts.h"
#include "ui/MarkdownHighlighter.h"

namespace nexus::ui {

class NoteEditor::Gutter : public QWidget {
public:
    explicit Gutter(NoteEditor* editor) : QWidget(editor), m_editor(editor) {}

protected:
    void paintEvent(QPaintEvent* event) override { m_editor->paintGutter(event); }

private:
    NoteEditor* m_editor;
};

NoteEditor::NoteEditor(const Palette& palette, QWidget* parent)
    : QPlainTextEdit(parent), m_palette(palette), m_gutter(new Gutter(this))
{
    setObjectName(QStringLiteral("noteEditor"));
    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QFont body = font();
    body.setPixelSize(MarkdownHighlighter::kBodyPixelSize);
    setFont(body);
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);

    connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { updateMargins(); });
    connect(this, &QPlainTextEdit::updateRequest, this, &NoteEditor::updateGutter);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &NoteEditor::updateCurrentLine);

    setPalette(palette);
    updateMargins();
}

void NoteEditor::showDocument(QTextDocument* document)
{
    setDocument(document);
    document->setDefaultFont(font());
    updateCurrentLine();
}

void NoteEditor::changeEvent(QEvent* event)
{
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        document()->setDefaultFont(font());
        setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    }
}

void NoteEditor::setPalette(const Palette& palette)
{
    m_palette = palette;
    QPalette qp = QPlainTextEdit::palette();
    qp.setColor(QPalette::Base, m_palette.surface);
    qp.setColor(QPalette::Text, m_palette.text);
    qp.setColor(QPalette::Highlight, m_palette.accentSoft);
    qp.setColor(QPalette::HighlightedText, m_palette.text);
    QPlainTextEdit::setPalette(qp);
    updateCurrentLine();
    m_gutter->update();
}

void NoteEditor::setLineNumbersVisible(bool visible)
{
    m_lineNumbers = visible;
    updateMargins();
    m_gutter->update();
}

int NoteEditor::cursorLine() const
{
    return textCursor().blockNumber() + 1;
}

int NoteEditor::cursorColumn() const
{
    return textCursor().positionInBlock() + 1;
}

void NoteEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    updateMargins();
}

void NoteEditor::focusOutEvent(QFocusEvent* event)
{
    QPlainTextEdit::focusOutEvent(event);
    emit focusLost();
}

void NoteEditor::updateMargins()
{
    // Centre the column (gutter + text + right padding) and keep the gutter left of the text.
    const QRect area = contentsRect();
    const int column = std::min(area.width(), kColumnMaxWidth);
    m_columnLeft = std::max(0, (area.width() - column) / 2);
    const int gutter = m_lineNumbers ? kGutterWidth : 0;
    const int left = m_columnLeft + gutter;
    const int right = std::max(0, area.width() - m_columnLeft - column) + kRightPadding;
    setViewportMargins(left, kTopPadding, right, 0);
    m_gutter->setVisible(m_lineNumbers);
    m_gutter->setGeometry(area.left() + m_columnLeft, area.top() + kTopPadding, gutter,
                          area.height() - kTopPadding);
}

void NoteEditor::updateCurrentLine()
{
    QTextEdit::ExtraSelection line;
    line.format.setBackground(m_palette.lineActive);
    line.format.setProperty(QTextFormat::FullWidthSelection, true);
    line.cursor = textCursor();
    line.cursor.clearSelection();
    setExtraSelections({line});
    m_gutter->update();
}

void NoteEditor::updateGutter(const QRect& rect, int dy)
{
    if (dy != 0) {
        m_gutter->scroll(0, dy);
    } else {
        m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
    }
}

void NoteEditor::paintGutter(QPaintEvent* event)
{
    QPainter painter(m_gutter);
    painter.fillRect(event->rect(), m_palette.surface);
    painter.setFont(fonts::monoFont(kLineNumberPixelSize));

    const int current = textCursor().blockNumber();
    const int numberRight = m_gutter->width() - kGutterPadding;
    QTextBlock block = firstVisibleBlock();
    const QPointF offset = contentOffset();
    while (block.isValid()) {
        const QRectF bounds = blockBoundingGeometry(block).translated(offset);
        if (bounds.top() > event->rect().bottom()) {
            break;
        }
        if (block.isVisible() && bounds.bottom() >= event->rect().top()) {
            const bool active = block.blockNumber() == current;
            const QRect row(0, static_cast<int>(bounds.top()), m_gutter->width(),
                            static_cast<int>(bounds.height()));
            if (active) {
                painter.fillRect(row, m_palette.lineActive);
            }
            // Align the number with the first line of the block (headings are taller).
            const QTextLayout* layout = block.layout();
            const int firstLine = layout != nullptr && layout->lineCount() > 0
                                      ? static_cast<int>(layout->lineAt(0).height())
                                      : row.height();
            painter.setPen(active ? m_palette.gutterActive : m_palette.gutter);
            painter.drawText(QRect(0, row.top(), numberRight, firstLine),
                             Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(block.blockNumber() + 1));
        }
        block = block.next();
    }
}

} // namespace nexus::ui
