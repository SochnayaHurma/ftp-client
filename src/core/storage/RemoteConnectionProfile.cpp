#include "RemoteConnectionProfile.h"

#include <QUrl>

namespace stl {

QString RemoteConnectionProfile::scheme() const {
    return kind == StorageKind::Ftp ? QStringLiteral("ftp") : QStringLiteral("sftp");
}

QString RemoteConnectionProfile::baseUrl() const {
    QString normalizedRoot = remoteRoot.trimmed();
    if (normalizedRoot.isEmpty()) {
        normalizedRoot = QStringLiteral("/");
    }
    if (!normalizedRoot.startsWith(QLatin1Char('/'))) {
        normalizedRoot.prepend(QLatin1Char('/'));
    }
    if (!normalizedRoot.endsWith(QLatin1Char('/'))) {
        normalizedRoot.append(QLatin1Char('/'));
    }

    QUrl url;
    url.setScheme(scheme());
    url.setHost(host.trimmed());
    if (port > 0) {
        url.setPort(port);
    }
    url.setPath(normalizedRoot);
    return url.toString(QUrl::FullyEncoded);
}

QString RemoteConnectionProfile::safeDescription() const {
    const QString userPart = username.trimmed().isEmpty()
        ? QStringLiteral("anonymous")
        : username.trimmed();
    return QStringLiteral("%1://%2@%3:%4%5")
        .arg(scheme(), userPart, host.trimmed())
        .arg(port)
        .arg(remoteRoot.trimmed().isEmpty() ? QStringLiteral("/") : remoteRoot.trimmed());
}

bool RemoteConnectionProfile::isValid(QString* error) const {
    if (kind != StorageKind::Ftp && kind != StorageKind::Sftp) {
        if (error) {
            *error = QStringLiteral("Для удалённого подключения выбран неподдерживаемый протокол");
        }
        return false;
    }

    if (host.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Не указан адрес сервера");
        }
        return false;
    }

    if (port <= 0 || port > 65535) {
        if (error) {
            *error = QStringLiteral("Порт должен находиться в диапазоне 1–65535");
        }
        return false;
    }

    return true;
}

}
