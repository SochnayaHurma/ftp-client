#pragma once

#include "StorageTypes.h"

#include <QString>
#include <QVector>

namespace stl {

class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    virtual QString backendName() const = 0;
    virtual StorageKind kind() const = 0;
    virtual bool isRemote() const = 0;

    virtual QString root() const = 0;
    virtual void setRoot(const QString& rootPath) = 0;

    virtual QString absolutePath(const QString& relativePath) const = 0;
    virtual QString relativePath(const QString& absolutePath) const = 0;

    virtual StorageObjectInfo objectInfo(const QString& absolutePath, bool calculateHash) const = 0;
    virtual QVector<StorageObjectInfo> enumerate(const QString& absolutePath,
                                                 bool recursive,
                                                 bool calculateHash) const = 0;

    virtual bool exists(const QString& absolutePath) const = 0;
    virtual bool makeDirectory(const QString& absolutePath, QString* error = nullptr) const = 0;
    virtual bool removeFile(const QString& absolutePath, QString* error = nullptr) const = 0;

    virtual bool writeFileFromLocal(const QString& localSourcePath,
                                    const QString& destinationPath,
                                    const QDateTime& modified,
                                    QString* error = nullptr) const = 0;

    virtual bool readFileToLocal(const QString& sourcePath,
                                 const QString& localDestinationPath,
                                 QString* error = nullptr) const = 0;
};

} // namespace stl
