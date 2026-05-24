#include "TransferManager.h"
#include "storage/LocalStorageBackend.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QtGlobal>

#include <utility>

namespace stl {

namespace {

qint64 transferableBytes(const QVector<PreflightItem>& plan) {
    qint64 total = 0;
    for (const PreflightItem& item : plan) {
        if (!item.isDirectory
            && item.status != PreflightStatus::Same
            && item.status != PreflightStatus::Error
            && item.status != PreflightStatus::Conflict) {
            total += qMax<qint64>(0, item.sourceSize);
        }
    }
    return total;
}

bool notifyProgress(const TransferProgressCallback& callback,
                    const TransferProgress& progress) {
    if (!callback) {
        return true;
    }
    return callback(progress);
}

} // namespace

TransferResult TransferManager::execute(const QVector<PreflightItem>& plan,
                                        const QString& destinationRoot,
                                        bool createBackup,
                                        TransferProgressCallback progressCallback) const {
    LocalStorageBackend destinationBackend(destinationRoot);

    QString sourceRoot;
    for (const PreflightItem& item : plan) {
        if (!item.sourcePath.isEmpty()) {
            sourceRoot = item.sourcePath.left(item.sourcePath.size() - item.relativePath.size());
            break;
        }
    }
    LocalStorageBackend sourceBackend(sourceRoot.isEmpty() ? QStringLiteral(".") : sourceRoot);
    return execute(plan, sourceBackend, destinationBackend, createBackup, std::move(progressCallback));
}

TransferResult TransferManager::execute(const QVector<PreflightItem>& plan,
                                        const IStorageBackend& sourceBackend,
                                        const IStorageBackend& destinationBackend,
                                        bool createBackup,
                                        TransferProgressCallback progressCallback) const {
    TransferResult result;
    const int totalItems = plan.size();
    const qint64 totalBytes = transferableBytes(plan);
    qint64 processedBytes = 0;
    int processedItems = 0;

    auto progress = [&](int index, QueueItemStatus status, const QString& message) {
        TransferProgress info;
        info.itemIndex = index;
        info.totalItems = totalItems;
        info.processedItems = processedItems;
        info.totalBytes = totalBytes;
        info.processedBytes = processedBytes;
        info.status = status;
        info.relativePath = index >= 0 && index < plan.size() ? plan[index].relativePath : QString();
        info.message = message;
        return notifyProgress(progressCallback, info);
    };

    for (int index = 0; index < plan.size(); ++index) {
        const PreflightItem& item = plan[index];

        if (!progress(index, QueueItemStatus::Running, QStringLiteral("Операция запущена"))) {
            result.canceled = true;
            result.messages.push_back(QStringLiteral("Передача отменена пользователем до выполнения: %1").arg(item.relativePath));
            progress(index, QueueItemStatus::Canceled, QStringLiteral("Отменено пользователем"));
            break;
        }

        if (item.status == PreflightStatus::Error || item.status == PreflightStatus::Conflict) {
            result.errors++;
            processedItems++;
            const QString message = QStringLiteral("Операция заблокирована: %1 — %2").arg(item.relativePath, item.warning);
            result.messages.push_back(message);
            if (!progress(index, QueueItemStatus::Error, message)) {
                result.canceled = true;
                break;
            }
            continue;
        }

        if (item.status == PreflightStatus::Same) {
            result.skipped++;
            processedItems++;
            const QString message = QStringLiteral("Пропуск без изменений: %1").arg(item.relativePath);
            result.messages.push_back(message);
            if (!progress(index, QueueItemStatus::Skipped, message)) {
                result.canceled = true;
                break;
            }
            continue;
        }

        if (item.isDirectory) {
            QString error;
            processedItems++;
            if (destinationBackend.makeDirectory(item.destinationPath, &error)) {
                result.copied++;
                const QString message = QStringLiteral("Папка создана: %1").arg(item.relativePath);
                result.messages.push_back(message);
                if (!progress(index, QueueItemStatus::Done, message)) {
                    result.canceled = true;
                    break;
                }
            } else {
                result.errors++;
                const QString message = error.isEmpty()
                    ? QStringLiteral("Не удалось создать папку: %1").arg(item.relativePath)
                    : error;
                result.messages.push_back(message);
                if (!progress(index, QueueItemStatus::Error, message)) {
                    result.canceled = true;
                    break;
                }
            }
            continue;
        }

        const bool ok = copyFileThroughBackends(item, sourceBackend, destinationBackend, createBackup, result.messages);
        processedItems++;
        if (ok) {
            processedBytes += qMax<qint64>(0, item.sourceSize);
            result.copied++;
            const QString message = QStringLiteral("Файл передан: %1").arg(item.relativePath);
            if (!progress(index, QueueItemStatus::Done, message)) {
                result.canceled = true;
                break;
            }
        } else {
            result.errors++;
            const QString message = result.messages.isEmpty()
                ? QStringLiteral("Ошибка передачи файла: %1").arg(item.relativePath)
                : result.messages.last();
            if (!progress(index, QueueItemStatus::Error, message)) {
                result.canceled = true;
                break;
            }
        }
    }

    if (result.canceled) {
        result.messages.push_back(QStringLiteral("Очередь передачи остановлена пользователем"));
    }

    return result;
}

bool TransferManager::copyFileThroughBackends(const PreflightItem& item,
                                              const IStorageBackend& sourceBackend,
                                              const IStorageBackend& destinationBackend,
                                              bool createBackup,
                                              QStringList& messages) const {
    QString error;

    if (destinationBackend.exists(item.destinationPath)) {
        if (createBackup && !backupExistingFile(item, destinationBackend, messages)) {
            return false;
        }

        if (!destinationBackend.removeFile(item.destinationPath, &error)) {
            messages.push_back(error.isEmpty()
                ? QStringLiteral("Не удалось удалить старый файл перед перезаписью: %1").arg(item.relativePath)
                : error);
            return false;
        }
    }

    bool ok = false;

    if (!sourceBackend.isRemote()) {
        ok = destinationBackend.writeFileFromLocal(item.sourcePath, item.destinationPath, item.sourceModified, &error);
    } else {
        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            messages.push_back(QStringLiteral("Не удалось создать временный каталог для передачи: %1").arg(item.relativePath));
            return false;
        }

        const QString tempFile = tempDir.filePath(QStringLiteral("payload.bin"));
        if (!sourceBackend.readFileToLocal(item.sourcePath, tempFile, &error)) {
            messages.push_back(error.isEmpty()
                ? QStringLiteral("Не удалось получить файл из удалённого backend'а: %1").arg(item.relativePath)
                : error);
            return false;
        }

        ok = destinationBackend.writeFileFromLocal(tempFile, item.destinationPath, item.sourceModified, &error);
    }

    if (!ok) {
        messages.push_back(error.isEmpty()
            ? QStringLiteral("Не удалось передать файл: %1").arg(item.relativePath)
            : error);
        return false;
    }

    messages.push_back(QStringLiteral("Файл передан: %1").arg(item.relativePath));
    return true;
}

bool TransferManager::backupExistingFile(const PreflightItem& item,
                                         const IStorageBackend& destinationBackend,
                                         QStringList& messages) const {
    if (!destinationBackend.exists(item.destinationPath)) {
        return true;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        messages.push_back(QStringLiteral("Не удалось создать временный каталог для резервной копии: %1").arg(item.relativePath));
        return false;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString backupPath = destinationBackend.absolutePath(
        QStringLiteral(".stl_backup/%1/%2").arg(stamp, item.relativePath));
    const QString tempBackup = tempDir.filePath(QStringLiteral("backup.bin"));

    QString error;
    if (!destinationBackend.readFileToLocal(item.destinationPath, tempBackup, &error)) {
        messages.push_back(error.isEmpty()
            ? QStringLiteral("Не удалось прочитать файл назначения для резервной копии: %1").arg(item.relativePath)
            : error);
        return false;
    }

    if (!destinationBackend.writeFileFromLocal(tempBackup, backupPath, item.destinationModified, &error)) {
        messages.push_back(error.isEmpty()
            ? QStringLiteral("Не удалось создать резервную копию: %1").arg(item.relativePath)
            : error);
        return false;
    }

    messages.push_back(QStringLiteral("Резервная копия создана: %1").arg(backupPath));
    return true;
}

} // namespace stl
