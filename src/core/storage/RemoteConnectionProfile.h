#pragma once

#include "StorageTypes.h"

#include <QString>

namespace stl {

struct RemoteConnectionProfile {
    StorageKind kind = StorageKind::Sftp;
    QString host;
    int port = 22;
    QString username;
    QString password;
    QString remoteRoot = QStringLiteral("/");
    bool passiveMode = true;
    int timeoutSeconds = 10;

    QString scheme() const;
    QString baseUrl() const;
    QString safeDescription() const;
    bool isValid(QString* error = nullptr) const;
};

} // namespace stl
