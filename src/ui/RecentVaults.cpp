#include "ui/RecentVaults.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace nexus::ui {

namespace {

constexpr auto kListKey = "recent/vaults";
constexpr auto kLastKey = "recent/lastOpen";

#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseSensitive;
#endif

} // namespace

RecentVaults::RecentVaults(QSettings& settings) : m_settings(&settings) {}

QString RecentVaults::normalizedPath(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString RecentVaults::displayName(const QString& path)
{
    const QString clean = normalizedPath(path);
    QString name = clean.section(QLatin1Char('/'), -1, -1, QString::SectionSkipEmpty);
    return name.isEmpty() ? clean : name;
}

QList<RecentVault> RecentVaults::entries() const
{
    QList<RecentVault> result;
    const QByteArray json = m_settings->value(QLatin1StringView(kListKey)).toString().toUtf8();
    const QJsonArray array = QJsonDocument::fromJson(json).array();
    for (const auto& value : array) {
        const QJsonObject object = value.toObject();
        const QString path = normalizedPath(object.value(QLatin1String("path")).toString());
        if (path.isEmpty()) {
            continue;
        }
        result.push_back(
            {path,
             QDateTime::fromString(object.value(QLatin1String("opened")).toString(), Qt::ISODate)});
        if (result.size() == kMaxEntries) {
            break;
        }
    }
    return result;
}

void RecentVaults::store(const QList<RecentVault>& list)
{
    QJsonArray array;
    for (const RecentVault& entry : list) {
        QJsonObject object;
        object.insert(QLatin1String("path"), entry.path);
        object.insert(QLatin1String("opened"), entry.opened.toString(Qt::ISODate));
        array.push_back(object);
    }
    m_settings->setValue(QLatin1StringView(kListKey),
                         QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

void RecentVaults::add(const QString& path, const QDateTime& when)
{
    const QString clean = normalizedPath(path);
    if (clean.isEmpty()) {
        return;
    }
    QList<RecentVault> list = entries();
    list.removeIf([&clean](const RecentVault& e) { return e.path.compare(clean, kPathCase) == 0; });
    list.prepend({clean, when});
    while (list.size() > kMaxEntries) {
        list.removeLast();
    }
    store(list);
}

void RecentVaults::remove(const QString& path)
{
    const QString clean = normalizedPath(path);
    QList<RecentVault> list = entries();
    list.removeIf([&clean](const RecentVault& e) { return e.path.compare(clean, kPathCase) == 0; });
    store(list);
}

void RecentVaults::clear()
{
    m_settings->remove(QLatin1StringView(kListKey));
}

QString RecentVaults::lastOpenVault() const
{
    return m_settings->value(QLatin1StringView(kLastKey)).toString();
}

void RecentVaults::setLastOpenVault(const QString& path)
{
    const QString clean = normalizedPath(path);
    if (clean.isEmpty()) {
        m_settings->remove(QLatin1StringView(kLastKey));
    } else {
        m_settings->setValue(QLatin1StringView(kLastKey), clean);
    }
}

QString formatRelativeTime(const QDateTime& then, const QDateTime& now)
{
    if (!then.isValid()) {
        return {};
    }
    const qint64 days = then.date().daysTo(now.date());
    if (days < 0) {
        return then.date().toString(Qt::ISODate); // clock moved back; show the date instead
    }
    if (days == 0) {
        return QObject::tr("今天");
    }
    if (days == 1) {
        return QObject::tr("昨天");
    }
    if (days < 7) {
        return QObject::tr("%1 天前").arg(days);
    }
    if (days < 30) {
        return QObject::tr("%1 週前").arg(days / 7);
    }
    if (days < 365) {
        return QObject::tr("%1 個月前").arg(days / 30);
    }
    return QObject::tr("%1 年前").arg(days / 365);
}

} // namespace nexus::ui
