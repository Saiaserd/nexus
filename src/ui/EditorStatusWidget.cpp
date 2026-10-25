#include "ui/EditorStatusWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTextDocument>

#include "ui/EditorTabs.h"
#include "ui/NoteEditor.h"
#include "ui/NoteSession.h"
#include "ui/WordCount.h"

namespace nexus::ui {

namespace {

constexpr int kWordCountDelayMs = 150;

} // namespace

EditorStatusWidget::EditorStatusWidget(EditorTabs& tabs, QWidget* parent)
    : QWidget(parent), m_tabs(tabs), m_save(new QLabel(this)), m_cursor(new QLabel(this)),
      m_words(new QLabel(this)), m_encoding(new QLabel(this))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 12, 0); // design: footer padding 0 12px, gap 18px
    layout->setSpacing(18);
    for (QLabel* label : {m_save, m_cursor, m_words, m_encoding}) {
        layout->addWidget(label);
    }

    m_wordTimer.setSingleShot(true);
    m_wordTimer.setInterval(kWordCountDelayMs);
    connect(&m_wordTimer, &QTimer::timeout, this, &EditorStatusWidget::refreshWordCount);
    connect(&m_tabs, &EditorTabs::statusChanged, this, &EditorStatusWidget::refresh);
    connect(&m_tabs, &EditorTabs::currentChanged, this, &EditorStatusWidget::refreshWordCount);
    refresh();
    refreshWordCount();
}

void EditorStatusWidget::refresh()
{
    const NoteSession* session = m_tabs.currentSession();
    const NoteEditor* editor = m_tabs.currentEditor();
    setVisible(session != nullptr);
    if (session == nullptr || editor == nullptr) {
        return;
    }
    switch (session->saveState()) {
    case NoteSession::SaveState::Saved:
        m_save->setText(tr("已儲存"));
        break;
    case NoteSession::SaveState::Unsaved:
        m_save->setText(tr("未儲存"));
        break;
    case NoteSession::SaveState::Saving:
        m_save->setText(tr("儲存中…"));
        break;
    }
    m_cursor->setText(
        tr("第 %1 行，第 %2 欄").arg(editor->cursorLine()).arg(editor->cursorColumn()));
    m_encoding->setText(session->lineEnding() == core::LineEnding::Crlf
                            ? QStringLiteral("UTF-8 · CRLF")
                            : QStringLiteral("UTF-8 · LF"));
    m_wordTimer.start();
}

void EditorStatusWidget::refreshWordCount()
{
    const NoteSession* session = m_tabs.currentSession();
    if (session == nullptr) {
        return;
    }
    m_words->setText(tr("%1 字").arg(countWords(session->document()->toPlainText())));
}

QString EditorStatusWidget::summary() const
{
    if (!isVisibleTo(parentWidget())) {
        return {};
    }
    return QStringList{m_save->text(), m_cursor->text(), m_words->text(), m_encoding->text()}.join(
        QStringLiteral(" · "));
}

} // namespace nexus::ui
