#include "FormatUtils.h"

#include <QLocale>

namespace ui {

QString formatBytesRu(qint64 bytes) {
    if (bytes < 0) {
        return QStringLiteral("-");
    }

    if (bytes == 0) {
        return QStringLiteral("0 байт");
    }

    if (bytes < 1024) {
        return QStringLiteral("%1 байт").arg(bytes);
    }

    struct Unit {
        double factor;
        const char* suffix;
    };

    static const Unit units[] = {
        {1024.0, "КБ"},
        {1024.0 * 1024.0, "МБ"},
        {1024.0 * 1024.0 * 1024.0, "ГБ"},
        {1024.0 * 1024.0 * 1024.0 * 1024.0, "ТБ"}
    };

    double value = static_cast<double>(bytes);
    QString suffix = QStringLiteral("Б");

    for (const Unit& unit : units) {
        value = static_cast<double>(bytes) / unit.factor;
        suffix = QString::fromUtf8(unit.suffix);
        if (value < 1024.0 || suffix == QStringLiteral("ТБ")) {
            break;
        }
    }

    const int decimals = value < 10.0 ? 1 : 0;
    const QLocale locale(QLocale::Russian, QLocale::Russia);

    return QStringLiteral("%1 %2")
        .arg(locale.toString(value, 'f', decimals))
        .arg(suffix);
}

}
