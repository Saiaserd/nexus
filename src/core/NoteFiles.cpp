#include "core/NoteFiles.h"

#include <array>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include "core/PathOrder.h"

namespace nexus::core {

namespace {

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
constexpr bool kCaseInsensitiveFs = true;
#else
constexpr bool kCaseInsensitiveFs = false;
#endif

constexpr qsizetype kMaxNameBytes = 250;
constexpr int kMaxCreateAttempts = 100000;

FileError makeError(FileErrorKind kind, const QString& path)
{
    return FileError{kind, path, std::nullopt};
}

FileError nameFailure(const QString& path, NameError reason)
{
    return FileError{FileErrorKind::InvalidName, path, reason};
}

FileError deviceError(const QFileDevice& device, const QString& rel)
{
    switch (device.error()) {
    case QFileDevice::PermissionsError:
        return makeError(FileErrorKind::AccessDenied, rel);
    default:
        return makeError(FileErrorKind::IoError, rel);
    }
}

bool isReservedStem(const QString& name)
{
    static constexpr std::array<const char*, 4> kDevices = {"CON", "PRN", "AUX", "NUL"};
    const qsizetype dot = name.indexOf(u'.');
    QString stem = dot < 0 ? name : name.left(dot);
    while (stem.endsWith(u' ')) {
        stem.chop(1);
    }
    for (const char* device : kDevices) {
        if (stem.compare(QLatin1String(device), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    if (stem.size() == 4 && stem[3] >= u'1' && stem[3] <= u'9') {
        const QString prefix = stem.left(3);
        return prefix.compare(QLatin1String("COM"), Qt::CaseInsensitive) == 0 ||
               prefix.compare(QLatin1String("LPT"), Qt::CaseInsensitive) == 0;
    }
    return false;
}

QString parentOf(const QString& rel)
{
    const qsizetype slash = rel.lastIndexOf(u'/');
    return slash < 0 ? QString() : rel.left(slash);
}

QString joinRel(const QString& folderRel, const QString& name)
{
    return folderRel.isEmpty() ? name : folderRel + u'/' + name;
}

} // namespace

std::optional<NameError> validateName(const QString& name)
{
    if (name.isEmpty()) {
        return NameError::Empty;
    }
    if (name.toUtf8().size() > kMaxNameBytes) {
        return NameError::TooLong;
    }
    for (const QChar ch : name) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7F) {
            return NameError::ControlCharacter;
        }
    }
    static const QString kForbidden = QStringLiteral("\\/:*?\"<>|");
    for (const QChar ch : name) {
        if (kForbidden.contains(ch)) {
            return NameError::InvalidCharacter;
        }
    }
    if (name.endsWith(u' ') || name.endsWith(u'.')) {
        return NameError::TrailingSpaceOrDot;
    }
    if (isReservedStem(name)) {
        return NameError::ReservedName;
    }
    return std::nullopt;
}

NoteFiles::NoteFiles(const QString& vaultRoot) : m_root(QDir::cleanPath(vaultRoot)) {}

Expected<NoteDocument, FileError> NoteFiles::read(const QString& rel) const
{
    if (!isSafeRelPath(rel)) {
        return unexpected(makeError(FileErrorKind::InvalidPath, rel));
    }
    const QString abs = m_root + u'/' + rel;
    const QFileInfo info(abs);
    if (!info.exists()) {
        return unexpected(makeError(FileErrorKind::NotFound, rel));
    }
    if (!info.isFile()) {
        return unexpected(makeError(FileErrorKind::NotAFile, rel));
    }
    QFile file(abs);
    if (!file.open(QIODevice::ReadOnly)) {
        return unexpected(deviceError(file, rel));
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return unexpected(deviceError(file, rel));
    }
    auto doc = NoteDocument::decode(bytes);
    if (!doc) {
        return unexpected(makeError(FileErrorKind::InvalidEncoding, rel));
    }
    return *doc;
}

Expected<void, FileError> NoteFiles::write(const QString& rel, const NoteDocument& doc) const
{
    if (!isSafeRelPath(rel)) {
        return unexpected(makeError(FileErrorKind::InvalidPath, rel));
    }
    const QString abs = m_root + u'/' + rel;
    const QFileInfo info(abs);
    if (info.exists() && !info.isFile()) {
        return unexpected(makeError(FileErrorKind::NotAFile, rel));
    }
    if (!QFileInfo(info.absolutePath()).isDir()) {
        return unexpected(makeError(FileErrorKind::NotFound, rel));
    }
    const QByteArray bytes = doc.encode();
    QSaveFile file(abs);
    if (!file.open(QIODevice::WriteOnly)) {
        return unexpected(deviceError(file, rel));
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        return unexpected(deviceError(file, rel));
    }
    if (m_listener) {
        const QFileInfo written(abs);
        m_listener(NoteInfo{rel, written.size(), written.lastModified()});
    }
    return {};
}

Expected<QString, FileError> NoteFiles::createNote(const QString& folderRel,
                                                   const QString& baseName) const
{
    if (!folderRel.isEmpty() && !isSafeRelPath(folderRel)) {
        return unexpected(makeError(FileErrorKind::InvalidPath, folderRel));
    }
    if (const auto bad = validateName(baseName)) {
        return unexpected(nameFailure(baseName, *bad));
    }
    const QString folderAbs = folderRel.isEmpty() ? m_root : m_root + u'/' + folderRel;
    const QFileInfo folderInfo(folderAbs);
    if (!folderInfo.exists()) {
        return unexpected(makeError(FileErrorKind::NotFound, folderRel));
    }
    if (!folderInfo.isDir()) {
        return unexpected(makeError(FileErrorKind::NotADirectory, folderRel));
    }
    for (int n = 0; n < kMaxCreateAttempts; ++n) {
        const QString name =
            (n == 0 ? baseName : baseName + u' ' + QString::number(n)) + QStringLiteral(".md");
        QString rel = joinRel(folderRel, name);
        const QString abs = m_root + u'/' + rel;
        if (QFileInfo::exists(abs)) {
            continue;
        }
        QFile file(abs);
        if (file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            return rel;
        }
        if (!QFileInfo::exists(abs)) {
            return unexpected(deviceError(file, rel));
        }
        // Lost a race against another creator: try the next number.
    }
    return unexpected(makeError(FileErrorKind::AlreadyExists, joinRel(folderRel, baseName)));
}

Expected<QString, FileError> NoteFiles::createFolder(const QString& parentRel,
                                                     const QString& name) const
{
    if (!parentRel.isEmpty() && !isSafeRelPath(parentRel)) {
        return unexpected(makeError(FileErrorKind::InvalidPath, parentRel));
    }
    if (const auto bad = validateName(name)) {
        return unexpected(nameFailure(name, *bad));
    }
    const QString parentAbs = parentRel.isEmpty() ? m_root : m_root + u'/' + parentRel;
    const QFileInfo parentInfo(parentAbs);
    if (!parentInfo.exists()) {
        return unexpected(makeError(FileErrorKind::NotFound, parentRel));
    }
    if (!parentInfo.isDir()) {
        return unexpected(makeError(FileErrorKind::NotADirectory, parentRel));
    }
    QString rel = joinRel(parentRel, name);
    const QString abs = m_root + u'/' + rel;
    if (QFileInfo::exists(abs)) {
        return unexpected(makeError(FileErrorKind::AlreadyExists, rel));
    }
    if (!QDir().mkdir(abs)) {
        return unexpected(makeError(FileErrorKind::IoError, rel));
    }
    return rel;
}

Expected<QString, FileError> NoteFiles::rename(const QString& rel, const QString& newBaseName) const
{
    if (!isSafeRelPath(rel)) {
        return unexpected(makeError(FileErrorKind::InvalidPath, rel));
    }
    const QString oldAbs = m_root + u'/' + rel;
    const QFileInfo oldInfo(oldAbs);
    if (!oldInfo.exists()) {
        return unexpected(makeError(FileErrorKind::NotFound, rel));
    }
    if (const auto bad = validateName(newBaseName)) {
        return unexpected(nameFailure(newBaseName, *bad));
    }

    const bool isDir = oldInfo.isDir();
    const QString oldName = oldInfo.fileName();
    QString suffix;
    if (!isDir) {
        const QString ext = oldInfo.suffix();
        suffix = ext.isEmpty() ? QString() : u'.' + ext; // keeps ".md" / ".MD" as it was
    }
    const QString newName = newBaseName + suffix;
    QString newRel = joinRel(parentOf(rel), newName);
    if (newName == oldName) {
        return rel;
    }
    const QString newAbs = m_root + u'/' + newRel;

    const bool caseOnly = kCaseInsensitiveFs && newName.compare(oldName, Qt::CaseInsensitive) == 0;
    const auto doRename = [isDir](const QString& from, const QString& to) {
        return isDir ? QDir().rename(from, to) : QFile::rename(from, to);
    };

    if (caseOnly) {
        // The file system treats both names as the same entry, so go through a temporary name.
        for (int n = 0; n < 100; ++n) {
            const QString tmp = oldAbs + QStringLiteral(".nexus-rename-") + QString::number(n);
            if (QFileInfo::exists(tmp)) {
                continue;
            }
            if (!doRename(oldAbs, tmp)) {
                return unexpected(makeError(FileErrorKind::IoError, rel));
            }
            if (!doRename(tmp, newAbs)) {
                doRename(tmp, oldAbs); // best effort: restore the original name
                return unexpected(makeError(FileErrorKind::IoError, rel));
            }
            return newRel;
        }
        return unexpected(makeError(FileErrorKind::IoError, rel));
    }

    if (QFileInfo::exists(newAbs)) {
        return unexpected(makeError(FileErrorKind::AlreadyExists, newRel));
    }
    if (!doRename(oldAbs, newAbs)) {
        return unexpected(makeError(FileErrorKind::IoError, rel));
    }
    return newRel;
}

Expected<void, FileError> NoteFiles::moveToTrash(const QString& rel) const
{
    if (!isSafeRelPath(rel)) {
        return unexpected(makeError(FileErrorKind::InvalidPath, rel));
    }
    const QString abs = m_root + u'/' + rel;
    if (!QFileInfo::exists(abs)) {
        return unexpected(makeError(FileErrorKind::NotFound, rel));
    }
    if (!QFile::moveToTrash(abs)) {
        return unexpected(makeError(FileErrorKind::TrashFailed, rel));
    }
    return {};
}

} // namespace nexus::core
