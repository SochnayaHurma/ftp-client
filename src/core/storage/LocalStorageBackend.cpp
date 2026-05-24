#include "LocalStorageBackend.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QVector>

namespace stl {

LocalStorageBackend::LocalStorageBackend()
    : m_root(QDir::homePath()) {
}

LocalStorageBackend::LocalStorageBackend(const QString& rootPath) {
    setRoot(rootPath);
}

QString LocalStorageBackend::backendName() const {
    return QStringLiteral("LocalStorageBackend");
}

StorageKind LocalStorageBackend::kind() const {
    return StorageKind::LocalFilesystem;
}

bool LocalStorageBackend::isRemote() const {
    return false;
}

QString LocalStorageBackend::root() const {
    return m_root;
}

void LocalStorageBackend::setRoot(const QString& rootPath) {
    m_root = QFileInfo(rootPath).absoluteFilePath();
}

QString LocalStorageBackend::absolutePath(const QString& relativePath) const {
    return QDir(m_root).absoluteFilePath(relativePath);
}

QString LocalStorageBackend::relativePath(const QString& absolutePath) const {
    return QDir(m_root).relativeFilePath(QFileInfo(absolutePath).absoluteFilePath());
}

StorageObjectInfo LocalStorageBackend::objectInfo(const QString& absolutePath, bool calculateHash) const {
    QFileInfo info(absolutePath);
    StorageObjectInfo result;
    result.valid = true;
    result.exists = info.exists();
    result.absolutePath = info.absoluteFilePath();
    result.relativePath = relativePath(result.absolutePath);
    result.name = info.fileName();

    // Файлы, выбранные вне текущего корня backend'а, тоже являются валидным
    // источником операции: пользователь мог открыть справа целевую папку и
    // выбрать слева произвольный файл. В таком случае переносим объект в
    // целевую папку по его имени, а не блокируем анализ из-за "..".
    if (result.relativePath.startsWith(QStringLiteral(".."))) {
        result.relativePath = result.name.isEmpty() ? info.fileName() : result.name;
    }

    if (!result.exists) {
        return result;
    }

    result.isDirectory = info.isDir();
    result.size = info.isDir() ? 0 : info.size();
    result.modified = info.lastModified();

    if (calculateHash && info.isFile()) {
        result.sha256 = fileSha256(info.absoluteFilePath());
        if (result.sha256.isEmpty()) {
            result.valid = false;
            result.error = QStringLiteral("Не удалось вычислить SHA-256 файла");
        }
    }

    return result;
}

QVector<StorageObjectInfo> LocalStorageBackend::enumerate(const QString& absolutePath,
                                                          bool recursive,
                                                          bool calculateHash) const {
    QVector<StorageObjectInfo> result;
    const QFileInfo selectedInfo(absolutePath);
    StorageObjectInfo rootInfo = objectInfo(absolutePath, calculateHash);

    const QString selectedRelativeToBackend = relativePath(rootInfo.absolutePath);
    const bool selectedOutsideBackendRoot = selectedRelativeToBackend.startsWith(QStringLiteral(".."));

    if (selectedOutsideBackendRoot) {
        rootInfo.relativePath = selectedInfo.fileName();
    }

    result.push_back(rootInfo);

    if (!rootInfo.valid || !rootInfo.exists || !rootInfo.isDirectory) {
        return result;
    }

    const QDirIterator::IteratorFlags flags = recursive
        ? QDirIterator::Subdirectories
        : QDirIterator::NoIteratorFlags;

    QDirIterator iterator(absolutePath,
                          QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                          flags);

    const QDir selectedRootDir(selectedInfo.absoluteFilePath());

    while (iterator.hasNext()) {
        iterator.next();
        StorageObjectInfo childInfo = objectInfo(iterator.fileInfo().absoluteFilePath(), calculateHash);
        if (selectedOutsideBackendRoot) {
            const QString childRelative = selectedRootDir.relativeFilePath(childInfo.absolutePath);
            childInfo.relativePath = rootInfo.relativePath + QStringLiteral("/") + childRelative;
        }
        result.push_back(childInfo);
    }

    return result;
}

bool LocalStorageBackend::exists(const QString& absolutePath) const {
    return QFileInfo::exists(absolutePath);
}

bool LocalStorageBackend::makeDirectory(const QString& absolutePath, QString* error) const {
    QDir dir;
    if (dir.mkpath(absolutePath)) {
        return true;
    }

    if (error) {
        *error = QStringLiteral("Не удалось создать каталог: %1").arg(absolutePath);
    }
    return false;
}

bool LocalStorageBackend::removeFile(const QString& absolutePath, QString* error) const {
    if (!QFile::exists(absolutePath)) {
        return true;
    }

    if (QFile::remove(absolutePath)) {
        return true;
    }

    if (error) {
        *error = QStringLiteral("Не удалось удалить файл: %1").arg(absolutePath);
    }
    return false;
}

bool LocalStorageBackend::writeFileFromLocal(const QString& localSourcePath,
                                             const QString& destinationPath,
                                             const QDateTime& modified,
                                             QString* error) const {
    if (!ensureParentDirectory(destinationPath, error)) {
        return false;
    }

    if (QFile::exists(destinationPath) && !QFile::remove(destinationPath)) {
        if (error) {
            *error = QStringLiteral("Не удалось удалить старый файл перед записью: %1").arg(destinationPath);
        }
        return false;
    }

    if (!QFile::copy(localSourcePath, destinationPath)) {
        if (error) {
            *error = QStringLiteral("Не удалось скопировать файл из %1 в %2").arg(localSourcePath, destinationPath);
        }
        return false;
    }

    if (modified.isValid()) {
        QFile copied(destinationPath);
        copied.setFileTime(modified, QFileDevice::FileModificationTime);
    }

    return true;
}

bool LocalStorageBackend::readFileToLocal(const QString& sourcePath,
                                          const QString& localDestinationPath,
                                          QString* error) const {
    if (!ensureParentDirectory(localDestinationPath, error)) {
        return false;
    }

    if (QFile::exists(localDestinationPath) && !QFile::remove(localDestinationPath)) {
        if (error) {
            *error = QStringLiteral("Не удалось очистить временный файл: %1").arg(localDestinationPath);
        }
        return false;
    }

    if (!QFile::copy(sourcePath, localDestinationPath)) {
        if (error) {
            *error = QStringLiteral("Не удалось прочитать файл в локальную копию: %1").arg(sourcePath);
        }
        return false;
    }

    return true;
}

QString LocalStorageBackend::fileSha256(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }

    return QString::fromLatin1(hash.result().toHex());
}

bool LocalStorageBackend::ensureParentDirectory(const QString& filePath, QString* error) const {
    const QFileInfo info(filePath);
    QDir dir;
    if (dir.mkpath(info.absolutePath())) {
        return true;
    }

    if (error) {
        *error = QStringLiteral("Не удалось создать каталог назначения: %1").arg(info.absolutePath());
    }
    return false;
}

}
