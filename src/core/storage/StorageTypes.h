#pragma once

#include <QDateTime>
#include <QString>

namespace stl {

enum class StorageKind {
    LocalFilesystem,
    Ftp,
    Sftp
};

struct StorageObjectInfo {
    bool valid = false;
    bool exists = false;
    bool isDirectory = false;
    QString absolutePath;
    QString relativePath;
    QString name;
    qint64 size = 0;
    QDateTime modified;
    QString sha256;
    QString error;
};

inline QString storageKindToText(StorageKind kind) {
    switch (kind) {
    case StorageKind::LocalFilesystem:
        return QStringLiteral("Локальная файловая система");
    case StorageKind::Ftp:
        return QStringLiteral("FTP");
    case StorageKind::Sftp:
        return QStringLiteral("SFTP");
    }
    return QStringLiteral("Неизвестно");
}

} // namespace stl
