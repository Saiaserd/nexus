#include "ui/FileTreePanel.h"

#include <memory>

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QShortcut>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include "core/NoteFiles.h"
#include "core/Vault.h"
#include "ui/DeleteConfirmDialog.h"
#include "ui/ElidedLabel.h"
#include "ui/Icons.h"
#include "ui/RenameDelegate.h"
#include "ui/ThemeManager.h"
#include "ui/VaultSession.h"
#include "ui/VaultTreeModel.h"

namespace nexus::ui {

namespace {

constexpr auto kConfirmDeleteKey = "fileTree/confirmDelete";
constexpr int kHeaderHeight = 40;
constexpr int kHeaderIconSize = 14;
constexpr int kButtonIconSize = 15;
constexpr auto kNewFolderName = "新資料夾";
constexpr int kMaxFolderNameTries = 200;

using PathResult = Expected<QString, core::FileError>;

// Runs `work` on the thread pool; `done` is called on the UI thread with its result unless the
// owner is gone by then.
template <typename Result, typename Work, typename Done>
void runAsync(QObject* owner, Work work, Done done)
{
    auto* watcher = new QFutureWatcher<Result>(owner);
    QObject::connect(watcher, &QFutureWatcherBase::finished, owner,
                     [watcher, done = std::move(done)]() mutable {
                         done(watcher->result());
                         watcher->deleteLater();
                     });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

QString describeFileError(const core::FileError& error)
{
    switch (error.kind) {
    case core::FileErrorKind::NotFound:
        return QObject::tr("找不到這個項目，它可能已被移動或刪除。");
    case core::FileErrorKind::AlreadyExists:
        return QObject::tr("已經有同名的項目。");
    case core::FileErrorKind::AccessDenied:
        return QObject::tr("沒有權限。");
    case core::FileErrorKind::InvalidName:
        return error.nameError ? describeNameError(*error.nameError) : QObject::tr("名稱不合法。");
    case core::FileErrorKind::TrashFailed:
        return QObject::tr("無法移到資源回收筒。檔案沒有被刪除。");
    default:
        return QObject::tr("檔案操作失敗。");
    }
}

} // namespace

FileTreePanel::FileTreePanel(ThemeManager& theme, QWidget* parent)
    : QWidget(parent), m_theme(theme), m_model(new VaultTreeModel(this)),
      m_view(new QTreeView(this)), m_delegate(new RenameDelegate(theme, this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("sidebarHeader"));
    header->setFixedHeight(kHeaderHeight);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 6, 0);
    headerLayout->setSpacing(2);
    m_icon = new QLabel(header);
    m_icon->setFixedSize(kHeaderIconSize, kHeaderIconSize);
    headerLayout->addWidget(m_icon);
    m_name = new ElidedLabel(header);
    m_name->setObjectName(QStringLiteral("sidebarTitle"));
    m_name->setContentsMargins(4, 0, 0, 0);
    headerLayout->addWidget(m_name, 1);

    const auto makeButton = [header, headerLayout](const QString& tip, const QString& name) {
        auto* button = new QToolButton(header);
        button->setToolTip(tip);
        button->setAccessibleName(name);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::TabFocus);
        headerLayout->addWidget(button);
        return button;
    };
    m_newNote = makeButton(tr("新增筆記（Ctrl+N）"), tr("新增筆記"));
    m_newFolder = makeButton(tr("新增資料夾"), tr("新增資料夾"));
    m_collapse = makeButton(tr("全部收合"), tr("全部收合"));
    layout->addWidget(header);

    m_view->setObjectName(QStringLiteral("fileTree"));
    m_view->setModel(m_model);
    m_view->setItemDelegate(m_delegate);
    m_view->setHeaderHidden(true);
    m_view->setRootIsDecorated(false);
    m_view->setIndentation(0);
    m_view->setExpandsOnDoubleClick(false);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setUniformRowHeights(true);
    m_view->setMouseTracking(true);
    m_view->setAnimated(false);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setAccessibleName(tr("檔案樹"));
    layout->addWidget(m_view, 1);

    connect(m_newNote, &QToolButton::clicked, this, &FileTreePanel::newNote);
    connect(m_newFolder, &QToolButton::clicked, this, &FileTreePanel::newFolder);
    connect(m_collapse, &QToolButton::clicked, m_view, &QTreeView::collapseAll);
    connect(m_view, &QTreeView::clicked, this, &FileTreePanel::onClicked);
    connect(m_view, &QWidget::customContextMenuRequested, this, &FileTreePanel::showContextMenu);
    connect(m_delegate, &RenameDelegate::renameRequested, this, &FileTreePanel::renameNow);

    const auto addShortcut = [this](const QKeySequence& keys, auto&& handler) {
        auto* shortcut = new QShortcut(keys, m_view);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut, &QShortcut::activated, this, handler);
    };
    addShortcut(QKeySequence(Qt::Key_Return), [this] { activateCurrent(false); });
    addShortcut(QKeySequence(Qt::Key_Enter), [this] { activateCurrent(false); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), [this] { activateCurrent(true); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter), [this] { activateCurrent(true); });
    addShortcut(QKeySequence(Qt::Key_F2), [this] {
        if (const QString rel = currentRel(); !rel.isEmpty()) {
            beginRename(rel);
        }
    });
    addShortcut(QKeySequence(Qt::Key_Delete), [this] {
        if (const QString rel = currentRel(); !rel.isEmpty()) {
            deleteWithConfirmation(rel);
        }
    });

    connect(&m_theme, &ThemeManager::themeChanged, this, &FileTreePanel::applyIcons);
    applyIcons();
}

void FileTreePanel::applyIcons()
{
    const Palette& pal = m_theme.palette();
    const qreal dpr = devicePixelRatioF();
    m_icon->setPixmap(icons::pixmap(QStringLiteral("folder"), pal.accent, kHeaderIconSize, dpr));
    const auto setIcon = [&pal](QToolButton* button, const QString& name) {
        button->setIcon(icons::icon(name, pal.muted, kButtonIconSize));
        button->setIconSize(QSize(kButtonIconSize, kButtonIconSize));
    };
    setIcon(m_newNote, QStringLiteral("file-plus"));
    setIcon(m_newFolder, QStringLiteral("folder-plus"));
    setIcon(m_collapse, QStringLiteral("collapse-all"));
    m_view->viewport()->update();
}

void FileTreePanel::setVaultName(const QString& name)
{
    m_name->setFullText(name);
}

QString FileTreePanel::vaultName() const
{
    return m_name->fullText();
}

// ---- session ------------------------------------------------------------------------------

void FileTreePanel::setSession(VaultSession& session)
{
    m_session = &session;
    connect(&session, &VaultSession::opened, this, &FileTreePanel::onOpened);
    connect(&session, &VaultSession::closed, this, &FileTreePanel::onClosed);
    if (session.isOpen()) {
        onOpened();
    }
}

void FileTreePanel::onOpened()
{
    ++m_generation;
    m_model->setVault(m_session->snapshot());
    core::VaultWatcher* watcher = m_session->watcher();
    if (watcher == nullptr) {
        return;
    }
    connect(watcher, &core::VaultWatcher::notesAdded, this, [this](const QStringList& paths) {
        for (const QString& rel : paths) {
            m_model->addNote(rel);
        }
    });
    connect(watcher, &core::VaultWatcher::notesRemoved, this, [this](const QStringList& paths) {
        for (const QString& rel : paths) {
            // Nexus' own deletes and renames were applied before the watcher noticed them.
            if (m_model->removePath(rel)) {
                emit noteRemoved(rel);
            }
        }
    });
    connect(watcher, &core::VaultWatcher::foldersChanged, this, &FileTreePanel::reconcileFolders);
}

void FileTreePanel::onClosed()
{
    ++m_generation;
    m_model->setVault(core::Vault());
}

void FileTreePanel::reconcileFolders()
{
    if (m_session == nullptr || !m_session->isOpen()) {
        return;
    }
    const quint64 generation = m_generation;
    const QString root = m_session->root();
    runAsync<Expected<core::Vault, core::FileError>>(
        this, [root] { return core::Vault::open(root); },
        [this, generation](const Expected<core::Vault, core::FileError>& result) {
            if (generation != m_generation || !result) {
                return;
            }
            for (const QString& folder : result->folders()) {
                m_model->addFolder(folder);
            }
            for (const QString& folder : m_model->foldersUnder(QString())) {
                if (!result->contains(folder)) {
                    m_model->removePath(folder);
                }
            }
        });
}

// ---- selection and activation -------------------------------------------------------------

QString FileTreePanel::currentRel() const
{
    return m_model->relPath(m_view->currentIndex());
}

QString FileTreePanel::currentFolder() const
{
    const QString rel = currentRel();
    if (rel.isEmpty() || m_model->isFolder(rel)) {
        return rel;
    }
    return VaultTreeModel::parentPath(rel);
}

void FileTreePanel::expandTo(const QString& relPath)
{
    for (QString p = VaultTreeModel::parentPath(relPath); !p.isEmpty();
         p = VaultTreeModel::parentPath(p)) {
        m_view->expand(m_model->indexForPath(p));
    }
}

void FileTreePanel::selectNote(const QString& relPath)
{
    const QModelIndex index = m_model->indexForPath(relPath);
    if (!index.isValid()) {
        return;
    }
    expandTo(relPath);
    m_view->setCurrentIndex(index);
    m_view->scrollTo(index);
}

void FileTreePanel::setNoteDirty(const QString& relPath, bool dirty)
{
    m_model->setDirty(relPath, dirty);
}

void FileTreePanel::onClicked(const QModelIndex& index)
{
    if (m_model->isFolder(index)) {
        m_view->setExpanded(index, !m_view->isExpanded(index));
        return;
    }
    emit openNoteRequested(m_model->relPath(index),
                           (QGuiApplication::keyboardModifiers() & Qt::ControlModifier) != 0);
}

void FileTreePanel::activateCurrent(bool newTab)
{
    const QModelIndex index = m_view->currentIndex();
    if (!index.isValid()) {
        return;
    }
    if (m_model->isFolder(index)) {
        m_view->setExpanded(index, !m_view->isExpanded(index));
    } else {
        emit openNoteRequested(m_model->relPath(index), newTab);
    }
}

// ---- creating -----------------------------------------------------------------------------

void FileTreePanel::newNote()
{
    createNoteIn(currentFolder());
}

void FileTreePanel::newFolder()
{
    createFolderIn(currentFolder());
}

void FileTreePanel::createNoteIn(const QString& folderRel)
{
    const auto files = m_session != nullptr ? m_session->files() : nullptr;
    if (!files) {
        return;
    }
    const quint64 generation = m_generation;
    runAsync<PathResult>(
        this, [files, folderRel] { return files->createNote(folderRel); },
        [this, generation](const PathResult& result) {
            if (generation != m_generation) {
                return;
            }
            if (!result) {
                reportError(tr("無法新增筆記"), result.error());
                return;
            }
            const QString rel = *result;
            m_model->addNote(rel);
            selectNote(rel);
            emit openNoteRequested(rel, false);
            beginRename(rel);
        });
}

void FileTreePanel::createFolderIn(const QString& parentRel)
{
    const auto files = m_session != nullptr ? m_session->files() : nullptr;
    if (!files) {
        return;
    }
    const quint64 generation = m_generation;
    runAsync<PathResult>(
        this,
        [files, parentRel] {
            PathResult result = files->createFolder(parentRel, QString::fromUtf8(kNewFolderName));
            for (int i = 1; i <= kMaxFolderNameTries && !result &&
                            result.error().kind == core::FileErrorKind::AlreadyExists;
                 ++i) {
                result = files->createFolder(
                    parentRel,
                    QStringLiteral("%1 %2").arg(QString::fromUtf8(kNewFolderName)).arg(i));
            }
            return result;
        },
        [this, generation](const PathResult& result) {
            if (generation != m_generation) {
                return;
            }
            if (!result) {
                reportError(tr("無法新增資料夾"), result.error());
                return;
            }
            m_model->addFolder(*result);
            selectNote(*result);
            beginRename(*result);
        });
}

// ---- renaming -----------------------------------------------------------------------------

void FileTreePanel::beginRename(const QString& relPath)
{
    const QModelIndex index = m_model->indexForPath(relPath);
    if (!index.isValid()) {
        return;
    }
    expandTo(relPath);
    m_view->setCurrentIndex(index);
    m_view->scrollTo(index);
    m_view->setFocus();
    m_view->edit(index);
}

void FileTreePanel::renameNow(const QString& relPath, const QString& newName)
{
    const auto files = m_session != nullptr ? m_session->files() : nullptr;
    if (!files || !m_model->contains(relPath)) {
        return;
    }
    const quint64 generation = m_generation;
    runAsync<PathResult>(
        this, [files, relPath, newName] { return files->rename(relPath, newName); },
        [this, generation, relPath](const PathResult& result) {
            if (generation != m_generation) {
                return;
            }
            if (!result) {
                reportError(tr("無法重新命名"), result.error());
                return;
            }
            const QString newRel = *result;
            const QStringList notes = m_model->notesUnder(relPath);
            QStringList expanded;
            for (const QString& folder : m_model->foldersUnder(relPath)) {
                if (m_view->isExpanded(m_model->indexForPath(folder))) {
                    expanded << folder;
                }
            }
            const bool folderWasExpanded =
                m_model->isFolder(relPath) && m_view->isExpanded(m_model->indexForPath(relPath));
            const auto mapped = [&](const QString& old) {
                return newRel + old.mid(relPath.size());
            };

            m_model->renamePath(relPath, newRel);

            if (folderWasExpanded) {
                m_view->expand(m_model->indexForPath(newRel));
            }
            for (const QString& folder : expanded) {
                m_view->expand(m_model->indexForPath(mapped(folder)));
            }
            selectNote(newRel);
            for (const QString& note : notes) {
                emit noteRenamed(note, mapped(note));
            }
        });
}

// ---- deleting -----------------------------------------------------------------------------

void FileTreePanel::deleteWithConfirmation(const QString& relPath)
{
    if (!m_model->contains(relPath)) {
        return;
    }
    const bool confirm = m_settings == nullptr ||
                         m_settings->value(QLatin1StringView(kConfirmDeleteKey), true).toBool();
    if (confirm) {
        const QStringList notes = m_model->notesUnder(relPath);
        bool unsaved = false;
        for (const QString& note : notes) {
            unsaved =
                unsaved ||
                m_model->data(m_model->indexForPath(note), VaultTreeModel::DirtyRole).toBool();
        }
        const bool folder = m_model->isFolder(relPath);
        DeleteConfirmDialog dialog(VaultTreeModel::displayName(relPath, folder), folder,
                                   static_cast<int>(notes.size()), unsaved, window());
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        if (dialog.dontAskAgain() && m_settings != nullptr) {
            m_settings->setValue(QLatin1StringView(kConfirmDeleteKey), false);
        }
    }
    deleteNow(relPath);
}

void FileTreePanel::deleteNow(const QString& relPath)
{
    const auto files = m_session != nullptr ? m_session->files() : nullptr;
    if (!files) {
        return;
    }
    const quint64 generation = m_generation;
    using VoidResult = Expected<void, core::FileError>;
    runAsync<VoidResult>(
        this, [files, relPath] { return files->moveToTrash(relPath); },
        [this, generation, relPath](const VoidResult& result) {
            if (generation != m_generation) {
                return;
            }
            if (!result) {
                reportError(tr("無法刪除"), result.error());
                return;
            }
            const QStringList notes = m_model->notesUnder(relPath);
            m_model->removePath(relPath);
            for (const QString& note : notes) {
                emit noteRemoved(note);
            }
        });
}

void FileTreePanel::reportError(const QString& action, const core::FileError& error) const
{
    // Window-modal but not blocking: open() returns at once, the event loop keeps running.
    auto* box = new QMessageBox(QMessageBox::Warning, tr("Nexus"),
                                action + QStringLiteral("\n\n") + describeFileError(error),
                                QMessageBox::Ok, window());
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->open();
}

// ---- context menu -------------------------------------------------------------------------

void FileTreePanel::revealInFileManager(const QString& relPath) const
{
    if (m_session == nullptr) {
        return;
    }
    const QString absolute = QDir::cleanPath(m_session->root() + u'/' + relPath);
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,") + QDir::toNativeSeparators(absolute)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(absolute).absolutePath()));
#endif
}

void FileTreePanel::showContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_view->indexAt(pos);
    QMenu menu(this);
    if (!index.isValid()) {
        menu.addAction(tr("新增筆記"), this, [this] { createNoteIn(QString()); });
        menu.addAction(tr("新增資料夾"), this, [this] { createFolderIn(QString()); });
        menu.exec(m_view->viewport()->mapToGlobal(pos));
        return;
    }
    m_view->setCurrentIndex(index);
    const QString rel = m_model->relPath(index);
    const bool folder = m_model->isFolder(index);
    const QString parentFolder = folder ? rel : VaultTreeModel::parentPath(rel);

    if (!folder) {
        QAction* open = menu.addAction(tr("在新分頁開啟"), this,
                                       [this, rel] { emit openNoteRequested(rel, true); });
        open->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    }
    menu.addAction(folder ? tr("在此資料夾新增筆記") : tr("在同一資料夾新增筆記"), this,
                   [this, parentFolder] { createNoteIn(parentFolder); });
    if (folder) {
        menu.addAction(tr("在此資料夾新增資料夾"), this,
                       [this, parentFolder] { createFolderIn(parentFolder); });
    }
    menu.addSeparator();
    menu.addAction(tr("重新命名"), this, [this, rel] { beginRename(rel); })
        ->setShortcut(QKeySequence(Qt::Key_F2));
    menu.addAction(tr("複製相對路徑"), this, [rel] { QGuiApplication::clipboard()->setText(rel); });
    menu.addAction(tr("在檔案總管中顯示"), this, [this, rel] { revealInFileManager(rel); });
    menu.addSeparator();
    menu.addAction(tr("刪除"), this, [this, rel] { deleteWithConfirmation(rel); })
        ->setShortcut(QKeySequence(Qt::Key_Delete));
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

} // namespace nexus::ui
