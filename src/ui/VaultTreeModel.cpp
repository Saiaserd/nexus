#include "ui/VaultTreeModel.h"

#include <algorithm>

#include "core/PathOrder.h"

namespace nexus::ui {

namespace {

constexpr int kExtensionLength = 3; // ".md"

QString baseOf(const QString& rel)
{
    return rel.mid(rel.lastIndexOf(u'/') + 1);
}

} // namespace

VaultTreeModel::VaultTreeModel(QObject* parent)
    : QAbstractItemModel(parent), m_root(std::make_unique<Node>())
{
    m_root->folder = true;
}

VaultTreeModel::~VaultTreeModel() = default;

QString VaultTreeModel::parentPath(const QString& rel)
{
    const auto slash = rel.lastIndexOf(u'/');
    return slash < 0 ? QString() : rel.left(slash);
}

QString VaultTreeModel::displayName(const QString& rel, bool folder)
{
    const QString base = baseOf(rel);
    return folder ? base : base.chopped(std::min<qsizetype>(kExtensionLength, base.size()));
}

// ---- lookup -------------------------------------------------------------------------------

VaultTreeModel::Node* VaultTreeModel::find(const QString& rel) const
{
    return rel.isEmpty() ? m_root.get() : m_nodes.value(rel, nullptr);
}

VaultTreeModel::Node* VaultTreeModel::nodeFor(const QModelIndex& index) const
{
    return index.isValid() ? static_cast<Node*>(index.internalPointer()) : m_root.get();
}

int VaultTreeModel::rowOf(const Node* node) const
{
    const auto& siblings = node->parent->children;
    const auto it = std::find_if(siblings.begin(), siblings.end(),
                                 [node](const auto& child) { return child.get() == node; });
    return static_cast<int>(it - siblings.begin());
}

QModelIndex VaultTreeModel::indexOf(const Node* node) const
{
    if (node == nullptr || node == m_root.get()) {
        return {};
    }
    return createIndex(rowOf(node), 0, const_cast<Node*>(node));
}

bool VaultTreeModel::contains(const QString& rel) const
{
    return !rel.isEmpty() && m_nodes.contains(rel);
}

bool VaultTreeModel::isFolder(const QString& rel) const
{
    const Node* node = find(rel);
    return node != nullptr && node->folder;
}

QModelIndex VaultTreeModel::indexForPath(const QString& rel) const
{
    return indexOf(find(rel));
}

QString VaultTreeModel::relPath(const QModelIndex& index) const
{
    return index.isValid() ? nodeFor(index)->rel : QString();
}

bool VaultTreeModel::isFolder(const QModelIndex& index) const
{
    return !index.isValid() || nodeFor(index)->folder;
}

void VaultTreeModel::collect(const Node* node, QStringList& notes, QStringList* folders)
{
    if (!node->folder) {
        notes << node->rel;
        return;
    }
    for (const auto& child : node->children) {
        if (child->folder && folders != nullptr) {
            *folders << child->rel;
        }
        collect(child.get(), notes, folders);
    }
}

QStringList VaultTreeModel::notesUnder(const QString& rel) const
{
    QStringList notes;
    if (const Node* node = find(rel)) {
        collect(node, notes, nullptr);
    }
    return notes;
}

QStringList VaultTreeModel::foldersUnder(const QString& rel) const
{
    QStringList notes;
    QStringList folders;
    if (const Node* node = find(rel)) {
        collect(node, notes, &folders);
    }
    return folders;
}

int VaultTreeModel::countNotes(const Node* node)
{
    if (!node->folder) {
        return 1;
    }
    int total = 0;
    for (const auto& child : node->children) {
        total += countNotes(child.get());
    }
    return total;
}

bool VaultTreeModel::siblingNameTaken(const QModelIndex& index, const QString& baseName) const
{
    const Node* self = nodeFor(index);
    if (self == nullptr || self->parent == nullptr) {
        return false;
    }
    const QString wanted = self->folder ? baseName : baseName + QStringLiteral(".md");
    return std::any_of(self->parent->children.begin(), self->parent->children.end(),
                       [&](const auto& sibling) {
                           return sibling.get() != self &&
                                  baseOf(sibling->rel).compare(wanted, Qt::CaseInsensitive) == 0;
                       });
}

// ---- changes ------------------------------------------------------------------------------

VaultTreeModel::Node* VaultTreeModel::insertChild(Node* parent, const QString& rel, bool folder)
{
    auto node = std::make_unique<Node>();
    node->rel = rel;
    node->folder = folder;
    node->parent = parent;
    auto& siblings = parent->children;
    const auto pos =
        std::lower_bound(siblings.begin(), siblings.end(), node, [](const auto& a, const auto& b) {
            return core::pathLess(a->rel, a->folder, b->rel, b->folder);
        });
    const int row = static_cast<int>(pos - siblings.begin());
    Node* raw = node.get();
    if (!m_resetting) {
        beginInsertRows(indexOf(parent), row, row);
    }
    siblings.insert(pos, std::move(node));
    m_nodes.insert(rel, raw);
    if (!m_resetting) {
        endInsertRows();
    }
    return raw;
}

VaultTreeModel::Node* VaultTreeModel::ensureFolder(const QString& rel)
{
    if (rel.isEmpty()) {
        return m_root.get();
    }
    if (Node* existing = find(rel)) {
        return existing;
    }
    return insertChild(ensureFolder(parentPath(rel)), rel, true);
}

void VaultTreeModel::addNote(const QString& rel)
{
    if (rel.isEmpty() || contains(rel)) {
        return;
    }
    insertChild(ensureFolder(parentPath(rel)), rel, false);
}

void VaultTreeModel::addFolder(const QString& rel)
{
    ensureFolder(rel);
}

void VaultTreeModel::forget(const Node* node)
{
    m_nodes.remove(node->rel);
    for (const auto& child : node->children) {
        forget(child.get());
    }
}

bool VaultTreeModel::removePath(const QString& rel)
{
    Node* node = rel.isEmpty() ? nullptr : find(rel);
    if (node == nullptr) {
        return false;
    }
    Node* parent = node->parent;
    const int row = rowOf(node);
    beginRemoveRows(indexOf(parent), row, row);
    forget(node);
    parent->children.erase(parent->children.begin() + row);
    endRemoveRows();
    return true;
}

void VaultTreeModel::renamePath(const QString& oldRel, const QString& newRel)
{
    const Node* node = find(oldRel);
    if (node == nullptr || oldRel == newRel || oldRel.isEmpty() || newRel.isEmpty()) {
        return;
    }
    struct Moved {
        QString rel;
        bool folder;
        bool dirty;
    };
    std::vector<Moved> moved;
    const auto gather = [&moved, &oldRel, &newRel](const auto& self, const Node* n) -> void {
        moved.push_back({newRel + n->rel.mid(oldRel.size()), n->folder, n->dirty});
        for (const auto& child : n->children) {
            self(self, child.get());
        }
    };
    gather(gather, node);

    removePath(oldRel);
    // Parents come before their children, so each folder exists when its content arrives.
    for (const Moved& item : moved) {
        if (item.folder) {
            ensureFolder(item.rel);
        } else {
            addNote(item.rel);
            setDirty(item.rel, item.dirty);
        }
    }
}

void VaultTreeModel::setDirty(const QString& rel, bool dirty)
{
    Node* node = find(rel);
    if (node == nullptr || node == m_root.get() || node->dirty == dirty) {
        return;
    }
    node->dirty = dirty;
    const QModelIndex idx = indexOf(node);
    emit dataChanged(idx, idx, {DirtyRole});
}

void VaultTreeModel::setVault(const core::Vault& vault)
{
    beginResetModel();
    m_resetting = true;
    m_root->children.clear();
    m_nodes.clear();
    for (const QString& folder : vault.folders()) {
        ensureFolder(folder);
    }
    for (const auto& note : vault.notes()) {
        addNote(note.relPath);
    }
    m_resetting = false;
    endResetModel();
}

// ---- QAbstractItemModel -------------------------------------------------------------------

QModelIndex VaultTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0 || !hasIndex(row, column, parent)) {
        return {};
    }
    const Node* owner = nodeFor(parent);
    return createIndex(row, column, owner->children[static_cast<size_t>(row)].get());
}

QModelIndex VaultTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) {
        return {};
    }
    return indexOf(nodeFor(child)->parent);
}

int VaultTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0) {
        return 0;
    }
    return static_cast<int>(nodeFor(parent)->children.size());
}

int VaultTreeModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant VaultTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const Node* node = nodeFor(index);
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return displayName(node->rel, node->folder);
    case Qt::ToolTipRole:
        return node->rel;
    case RelPathRole:
        return node->rel;
    case IsFolderRole:
        return node->folder;
    case DirtyRole:
        return node->dirty;
    case NoteCountRole:
        return node->folder ? countNotes(node) : 0;
    default:
        return {};
    }
}

Qt::ItemFlags VaultTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

} // namespace nexus::ui
