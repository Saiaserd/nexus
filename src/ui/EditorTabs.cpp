#include "ui/EditorTabs.h"

#include <algorithm>
#include <utility>

#include <QAbstractButton>
#include <QApplication>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStackedWidget>
#include <QTabBar>
#include <QVBoxLayout>

#include "ui/Icons.h"
#include "ui/MarkdownHighlighter.h"
#include "ui/NoteEditor.h"
#include "ui/NoteSession.h"
#include "ui/ThemeManager.h"

namespace nexus::ui {

// Destruction order matters: the editor shows the session's document, so it goes first.
struct EditorTabs::Tab {
    std::unique_ptr<NoteSession> session;
    MarkdownHighlighter* highlighter = nullptr; // child of the session's document
    std::unique_ptr<NoteEditor> editor;
};

// Close button of a tab: a dot while the note is unsaved, the close cross on hover.
class EditorTabs::TabButton : public QAbstractButton {
public:
    TabButton(ThemeManager& theme, QWidget* parent) : QAbstractButton(parent), m_theme(theme)
    {
        setFixedSize(16, 16);
        setCursor(Qt::ArrowCursor);
        setToolTip(tr("關閉分頁"));
        setAttribute(Qt::WA_Hover);
    }

    void setDirty(bool dirty)
    {
        m_dirty = dirty;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const Palette& p = m_theme.palette();
        if (m_dirty && !underMouse()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(p.accent);
            painter.drawEllipse(QRectF(4.5, 4.5, 7, 7)); // design: 7px dot
            return;
        }
        const QPixmap cross = icons::pixmap(
            QStringLiteral("close"), underMouse() ? p.text : p.muted, 10, devicePixelRatioF());
        painter.drawPixmap(3, 3, cross);
    }

private:
    ThemeManager& m_theme;
    bool m_dirty = false;
};

namespace {

QString tabTitle(const QString& relPath)
{
    return QFileInfo(relPath).completeBaseName();
}

// The file tree's in-place rename editor commits or cancels when it loses focus, so opening a
// note right after creating it must not take the focus away from it.
void focusUnlessEditing(QWidget* editor)
{
    if (qobject_cast<QLineEdit*>(QApplication::focusWidget()) == nullptr) {
        editor->setFocus();
    }
}

} // namespace

EditorTabs::EditorTabs(ThemeManager& theme, QWidget* parent)
    : QWidget(parent), m_theme(theme), m_tabBar(new QTabBar(this)), m_breadcrumb(new QLabel(this)),
      m_stack(new QStackedWidget(this)),
      m_hint(new QLabel(tr("從左側選擇一篇筆記，或按 Ctrl+N 新增"), this))
{
    setObjectName(QStringLiteral("editorTabs"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabBar->setObjectName(QStringLiteral("editorTabBar"));
    m_tabBar->setDocumentMode(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setMovable(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setDrawBase(false);
    layout->addWidget(m_tabBar);

    m_breadcrumb->setObjectName(QStringLiteral("breadcrumb"));
    m_breadcrumb->setTextFormat(Qt::RichText);
    layout->addWidget(m_breadcrumb);

    m_hint->setObjectName(QStringLiteral("workspaceHint"));
    m_hint->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(m_hint);
    layout->addWidget(m_stack, 1);

    connect(m_tabBar, &QTabBar::currentChanged, this, &EditorTabs::onCurrentChanged);
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        auto tab = std::move(m_tabs[static_cast<std::size_t>(from)]);
        m_tabs.erase(m_tabs.begin() + from);
        m_tabs.insert(m_tabs.begin() + to, std::move(tab));
    });
    connect(&m_theme, &ThemeManager::themeChanged, this, &EditorTabs::applyTheme);
    onCurrentChanged(-1);
}

EditorTabs::~EditorTabs() = default;

void EditorTabs::applyTheme()
{
    const Palette& palette = m_theme.palette();
    for (const auto& tab : m_tabs) {
        tab->highlighter->setPalette(palette);
        tab->editor->setPalette(palette);
    }
    refreshBreadcrumb();
}

void EditorTabs::setVault(std::shared_ptr<const core::NoteFiles> files, const QString& vaultName)
{
    closeAll();
    m_files = std::move(files);
    m_vaultName = vaultName;
}

int EditorTabs::count() const
{
    return static_cast<int>(m_tabs.size());
}

int EditorTabs::indexOf(const QString& relPath) const
{
    for (std::size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->session->relPath() == relPath) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int EditorTabs::indexOf(const NoteSession* session) const
{
    for (std::size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->session.get() == session) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

NoteSession* EditorTabs::currentSession() const
{
    const int index = m_tabBar->currentIndex();
    return index >= 0 ? m_tabs[static_cast<std::size_t>(index)]->session.get() : nullptr;
}

NoteEditor* EditorTabs::currentEditor() const
{
    const int index = m_tabBar->currentIndex();
    return index >= 0 ? m_tabs[static_cast<std::size_t>(index)]->editor.get() : nullptr;
}

NoteSession* EditorTabs::sessionFor(const QString& relPath) const
{
    const int index = indexOf(relPath);
    return index >= 0 ? m_tabs[static_cast<std::size_t>(index)]->session.get() : nullptr;
}

void EditorTabs::openNote(const QString& relPath, bool newTab)
{
    if (m_files == nullptr) {
        return;
    }
    if (const int existing = indexOf(relPath); existing >= 0) {
        m_tabBar->setCurrentIndex(existing);
        return;
    }
    if (!newTab && m_tabBar->currentIndex() >= 0) {
        const int index = m_tabBar->currentIndex();
        closeTab(index);
        addTab(relPath);
        m_tabBar->moveTab(m_tabBar->count() - 1, std::min(index, m_tabBar->count() - 1));
        return;
    }
    addTab(relPath);
}

void EditorTabs::addTab(const QString& relPath)
{
    const Palette& palette = m_theme.palette();
    auto tab = std::make_unique<Tab>();
    tab->session = std::make_unique<NoteSession>(m_files, relPath);
    tab->highlighter = new MarkdownHighlighter(tab->session->document(), palette);
    tab->editor = std::make_unique<NoteEditor>(palette);
    tab->editor->showDocument(tab->session->document());
    tab->editor->setLineNumbersVisible(m_lineNumbers);
    tab->editor->setReadOnly(true); // until the text has been read
    m_stack->addWidget(tab->editor.get());

    NoteSession* session = tab->session.get();
    NoteEditor* editor = tab->editor.get();
    connect(session, &NoteSession::loaded, editor, [editor] {
        editor->setReadOnly(false);
        editor->moveCursor(QTextCursor::Start);
        if (editor->isVisible()) {
            focusUnlessEditing(editor);
        }
    });
    connect(session, &NoteSession::loadFailed, this, [this, session](const core::FileError&) {
        emit saveFailed(session->relPath(), tr("無法讀取這篇筆記。"));
    });
    connect(session, &NoteSession::saveStateChanged, this, [this, session] {
        if (const int index = indexOf(session); index >= 0) {
            refreshTab(index);
        }
        emit dirtyChanged(session->relPath(), session->isDirty());
        if (session == currentSession()) {
            emit statusChanged();
        }
    });
    connect(session, &NoteSession::saveFailed, this, [this, session](const core::FileError&) {
        emit saveFailed(session->relPath(), tr("儲存失敗，修改仍保留在編輯器中。"));
    });
    connect(editor, &NoteEditor::focusLost, session, &NoteSession::save);
    connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this, session] {
        if (session == currentSession()) {
            emit statusChanged();
        }
    });

    m_tabs.push_back(std::move(tab));
    const int index = m_tabBar->addTab(tabTitle(relPath));
    auto* button = new TabButton(m_theme, m_tabBar);
    connect(button, &QAbstractButton::clicked, this, [this, session] {
        if (const int i = indexOf(session); i >= 0) {
            closeTab(i);
        }
    });
    m_tabBar->setTabButton(index, QTabBar::RightSide, button);
    refreshTab(index);
    m_tabBar->setCurrentIndex(index);
    session->load();
}

void EditorTabs::refreshTab(int index)
{
    const auto& tab = m_tabs[static_cast<std::size_t>(index)];
    m_tabBar->setTabText(index, tabTitle(tab->session->relPath()));
    m_tabBar->setTabToolTip(index, tab->session->relPath());
    if (auto* button = dynamic_cast<TabButton*>(m_tabBar->tabButton(index, QTabBar::RightSide))) {
        button->setDirty(tab->session->isDirty());
    }
}

void EditorTabs::closeTab(int index)
{
    if (index < 0 || index >= count()) {
        return;
    }
    m_tabs[static_cast<std::size_t>(index)]->session->saveAndWait(); // may emit signals
    std::unique_ptr<Tab> tab = std::move(m_tabs[static_cast<std::size_t>(index)]);
    m_tabs.erase(m_tabs.begin() + index);
    m_stack->removeWidget(tab->editor.get());
    m_tabBar->removeTab(index); // may emit currentChanged for the neighbour
    const QString relPath = tab->session->relPath();
    const bool wasDirty = tab->session->isDirty();
    tab.reset();
    if (wasDirty) {
        emit dirtyChanged(relPath, false); // the tree must not keep showing the dot
    }
}

void EditorTabs::closeCurrentTab()
{
    closeTab(m_tabBar->currentIndex());
}

void EditorTabs::closeAll()
{
    while (count() > 0) {
        closeTab(count() - 1);
    }
}

void EditorTabs::saveCurrent()
{
    if (NoteSession* session = currentSession()) {
        session->save();
    }
}

bool EditorTabs::saveAllAndWait()
{
    bool ok = true;
    for (const auto& tab : m_tabs) {
        ok = tab->session->saveAndWait() && ok;
    }
    return ok;
}

void EditorTabs::renameNote(const QString& oldRel, const QString& newRel)
{
    // A folder rename moves every note below it.
    const QString oldPrefix = oldRel + QLatin1Char('/');
    for (std::size_t i = 0; i < m_tabs.size(); ++i) {
        NoteSession& session = *m_tabs[i]->session;
        if (session.relPath() == oldRel) {
            session.setRelPath(newRel);
        } else if (session.relPath().startsWith(oldPrefix)) {
            session.setRelPath(newRel + QLatin1Char('/') + session.relPath().mid(oldPrefix.size()));
        } else {
            continue;
        }
        refreshTab(static_cast<int>(i));
    }
    refreshBreadcrumb();
}

void EditorTabs::removeNote(const QString& relPath)
{
    // The file is gone (moved to the trash): close without writing it back.
    const QString prefix = relPath + QLatin1Char('/');
    for (int i = count() - 1; i >= 0; --i) {
        const QString& path = m_tabs[static_cast<std::size_t>(i)]->session->relPath();
        if (path == relPath || path.startsWith(prefix)) {
            std::unique_ptr<Tab> tab = std::move(m_tabs[static_cast<std::size_t>(i)]);
            m_tabs.erase(m_tabs.begin() + i);
            m_stack->removeWidget(tab->editor.get());
            m_tabBar->removeTab(i);
        }
    }
}

void EditorTabs::setLineNumbersVisible(bool visible)
{
    m_lineNumbers = visible;
    for (const auto& tab : m_tabs) {
        tab->editor->setLineNumbersVisible(visible);
    }
}

void EditorTabs::onCurrentChanged(int index)
{
    // Switching away saves the tab that was left (ADR 0006).
    for (std::size_t i = 0; i < m_tabs.size(); ++i) {
        if (static_cast<int>(i) != index) {
            m_tabs[i]->session->save();
        }
    }
    const bool hasTab = index >= 0 && index < count();
    m_tabBar->setVisible(count() > 0);
    m_breadcrumb->setVisible(hasTab);
    if (hasTab) {
        NoteEditor* editor = m_tabs[static_cast<std::size_t>(index)]->editor.get();
        m_stack->setCurrentWidget(editor);
        focusUnlessEditing(editor);
    } else {
        m_stack->setCurrentWidget(m_hint);
    }
    refreshBreadcrumb();
    emit currentChanged();
    emit statusChanged();
}

QString EditorTabs::breadcrumbText() const
{
    const NoteSession* session = currentSession();
    if (session == nullptr) {
        return {};
    }
    QStringList parts{m_vaultName};
    parts.append(session->relPath().split(QLatin1Char('/')));
    return parts.join(QStringLiteral(" › "));
}

void EditorTabs::refreshBreadcrumb()
{
    const NoteSession* session = currentSession();
    if (session == nullptr) {
        m_breadcrumb->clear();
        return;
    }
    // Design: muted segments, the file name in the text colour, '›' between them.
    const Palette& p = m_theme.palette();
    QStringList parts{m_vaultName};
    parts.append(session->relPath().split(QLatin1Char('/')));
    QString html;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const bool last = i == parts.size() - 1;
        if (i > 0) {
            html += QStringLiteral("&nbsp;&nbsp;›&nbsp;&nbsp;");
        }
        html += last ? QStringLiteral("<span style=\"color:%1\">%2</span>")
                           .arg(p.text.name(), parts[i].toHtmlEscaped())
                     : parts[i].toHtmlEscaped();
    }
    m_breadcrumb->setText(html);
}

} // namespace nexus::ui
