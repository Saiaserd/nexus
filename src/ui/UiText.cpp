#include "ui/UiText.h"

#include <QDir>
#include <QObject>

namespace nexus::ui {

QString formatWindowTitle(const QString& noteName, const QString& vaultName)
{
    QStringList parts;
    if (!noteName.isEmpty()) {
        parts << noteName;
    }
    if (!vaultName.isEmpty()) {
        parts << vaultName;
    }
    parts << QStringLiteral("Nexus");
    return parts.join(QStringLiteral(" — "));
}

QString describeOpenError(const VaultOpenError& error)
{
    const QString path = QDir::toNativeSeparators(error.file.path);
    if (error.folderNotEmpty) {
        return QObject::tr("「%1」不是空資料夾。建立新 vault 需要一個空資料夾。").arg(path);
    }
    switch (error.file.kind) {
    case core::FileErrorKind::NotFound:
        return QObject::tr("找不到資料夾「%1」。").arg(path);
    case core::FileErrorKind::NotADirectory:
        return QObject::tr("「%1」不是資料夾。").arg(path);
    case core::FileErrorKind::AccessDenied:
        return QObject::tr("沒有權限存取「%1」。").arg(path);
    case core::FileErrorKind::AlreadyExists:
        return QObject::tr("「%1」已經存在。").arg(path);
    case core::FileErrorKind::NotAFile:
    case core::FileErrorKind::InvalidPath:
    case core::FileErrorKind::InvalidName:
    case core::FileErrorKind::InvalidEncoding:
    case core::FileErrorKind::TrashFailed:
    case core::FileErrorKind::IoError:
        break;
    }
    return QObject::tr("無法開啟「%1」，讀寫時發生錯誤。").arg(path);
}

} // namespace nexus::ui
