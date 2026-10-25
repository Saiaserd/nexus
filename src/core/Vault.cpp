#include "core/Vault.h"

#include <algorithm>
#include <utility>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include "core/PathOrder.h"

namespace nexus::core {

namespace {

bool isMarkdown(const QString& name)
{
    return name.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive);
}

void scan(const QString& absDir, const QString& relDir, std::vector<NoteInfo>& notes,
          std::vector<QString>& folders)
{
    QDirIterator it(absDir, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::System);
    std::vector<std::pair<QString, QString>> subDirs; // {absolute, relative}
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString name = info.fileName();
        if (name.startsWith(u'.')) {
            continue;
        }
        const QString rel = relDir.isEmpty() ? name : relDir + u'/' + name;
        if (info.isDir()) {
            if (info.isSymLink()) {
                continue; // never follow directory links (loops)
            }
            folders.push_back(rel);
            subDirs.emplace_back(absDir + u'/' + name, rel);
        } else if (isMarkdown(name)) {
            notes.push_back(NoteInfo{rel, info.size(), info.lastModified()});
        }
    }
    for (const auto& [subAbs, subRel] : subDirs) {
        scan(subAbs, subRel, notes, folders);
    }
}

} // namespace

Expected<Vault, FileError> Vault::open(const QString& root)
{
    const QFileInfo rootInfo(root);
    if (root.isEmpty() || !rootInfo.exists()) {
        return unexpected(FileError{FileErrorKind::NotFound, root, std::nullopt});
    }
    if (!rootInfo.isDir()) {
        return unexpected(FileError{FileErrorKind::NotADirectory, root, std::nullopt});
    }
    if (!rootInfo.isReadable()) {
        return unexpected(FileError{FileErrorKind::AccessDenied, root, std::nullopt});
    }

    Vault vault;
    vault.m_root = QDir::cleanPath(rootInfo.absoluteFilePath());
    scan(vault.m_root, QString(), vault.m_notes, vault.m_folders);

    std::sort(vault.m_notes.begin(), vault.m_notes.end(), [](const NoteInfo& a, const NoteInfo& b) {
        return pathLess(a.relPath, false, b.relPath, false);
    });
    std::sort(vault.m_folders.begin(), vault.m_folders.end(),
              [](const QString& a, const QString& b) { return pathLess(a, true, b, true); });

    vault.m_known.reserve(static_cast<qsizetype>(vault.m_notes.size() + vault.m_folders.size()));
    for (const NoteInfo& note : vault.m_notes) {
        vault.m_known.insert(note.relPath);
    }
    for (const QString& folder : vault.m_folders) {
        vault.m_known.insert(folder);
    }
    return vault;
}

QString Vault::absolutePath(const QString& rel) const
{
    if (m_root.isEmpty() || !isSafeRelPath(rel)) {
        return {};
    }
    return m_root + u'/' + rel;
}

bool Vault::contains(const QString& rel) const
{
    return m_known.contains(rel);
}

} // namespace nexus::core
