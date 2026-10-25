#include "ui/Icons.h"

#include <QByteArray>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace nexus::ui::icons {

QPixmap pixmap(const QString& name, const QColor& color, int logicalSize, qreal devicePixelRatio)
{
    QFile file(QStringLiteral(":/ui/icons/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());

    QSvgRenderer renderer(svg);
    const int device = qRound(logicalSize * devicePixelRatio);
    QPixmap result(device, device);
    result.fill(Qt::transparent);
    result.setDevicePixelRatio(devicePixelRatio);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, logicalSize, logicalSize));
    return result;
}

QIcon icon(const QString& name, const QColor& color, int logicalSize)
{
    QIcon result;
    result.addPixmap(pixmap(name, color, logicalSize, 1.0));
    result.addPixmap(pixmap(name, color, logicalSize, 2.0));
    return result;
}

} // namespace nexus::ui::icons
