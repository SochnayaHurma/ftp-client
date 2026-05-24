#pragma once

#include "IStorageBackend.h"
#include "RemoteConnectionProfile.h"

#include <QVector>
#include <QString>

namespace stl {

class CurlRemoteStorage final : public IStorageBackend {
public:
    CurlRemoteStorage();
    explicit CurlRemoteStorage(const RemoteConnectionProfile& profile);

    static bool isBuiltWithCurl();

    void setProfile(const RemoteConnectionProfile& profile);
    RemoteConnectionProfile profile() const;
    bool testConnection(QString* error = nullptr) const;

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
    QString normalizeRemotePath(const QString& path) const;
    QString makeUrl(const QString& remotePath, bool directoryUrl = false) const;
    QString fileSha256(const QString& localPath) const;
    bool ensureRemoteParentDirectories(const QString& remotePath, QString* error) const;
    QVector<QString> parentDirectories(const QString& remotePath) const;
    StorageObjectInfo unavailableInfo(const QString& path, const QString& message) const;

    RemoteConnectionProfile m_profile;
    QString m_root = QStringLiteral("/");
};

} // namespace stl
