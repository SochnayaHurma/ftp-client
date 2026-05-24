#pragma once

#include "IStorageBackend.h"

#include <QDir>
#include <QString>

namespace stl {

class LocalStorageBackend final : public IStorageBackend {
public:
    LocalStorageBackend();
    explicit LocalStorageBackend(const QString& rootPath);

    QString backendName() const override;
    StorageKind kind() const override;
    bool isRemote() const override;

    QString root() const override;
    void setRoot(const QString& rootPath) override;

    QString absolutePath(const QString& relativePath) const override;
    QString relativePath(const QString& absolutePath) const override;

    StorageObjectInfo objectInfo(const QString& absolutePath, bool calculateHash) const override;
    QVector<StorageObjectInfo> enumerate(const QString& absolutePath,
                                         bool recursive,
                                         bool calculateHash) const override;

    bool exists(const QString& absolutePath) const override;
    bool makeDirectory(const QString& absolutePath, QString* error = nullptr) const override;
    bool removeFile(const QString& absolutePath, QString* error = nullptr) const override;

    bool writeFileFromLocal(const QString& localSourcePath,
                            const QString& destinationPath,
                            const QDateTime& modified,
                            QString* error = nullptr) const override;

    bool readFileToLocal(const QString& sourcePath,
                         const QString& localDestinationPath,
                         QString* error = nullptr) const override;

private:
    QString fileSha256(const QString& filePath) const;
    bool ensureParentDirectory(const QString& filePath, QString* error) const;

    QString m_root;
};

} // namespace stl
