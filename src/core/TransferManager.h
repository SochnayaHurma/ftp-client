#pragma once

#include "TransferQueue.h"
#include "TransferTypes.h"
#include "storage/IStorageBackend.h"

#include <functional>

#include <QVector>
#include <QStringList>

namespace stl {

struct TransferProgress {
    int itemIndex = -1;
    int totalItems = 0;
    int processedItems = 0;
    qint64 totalBytes = 0;
    qint64 processedBytes = 0;
    QueueItemStatus status = QueueItemStatus::Waiting;
    QString relativePath;
    QString message;
};

using TransferProgressCallback = std::function<bool(const TransferProgress&)>;

struct TransferResult {
    int copied = 0;
    int skipped = 0;
    int errors = 0;
    bool canceled = false;
    QStringList messages;
};

class TransferManager {
public:
    TransferResult execute(const QVector<PreflightItem>& plan,
                           const QString& destinationRoot,
                           bool createBackup,
                           TransferProgressCallback progressCallback = {}) const;

    TransferResult execute(const QVector<PreflightItem>& plan,
                           const IStorageBackend& sourceBackend,
                           const IStorageBackend& destinationBackend,
                           bool createBackup,
                           TransferProgressCallback progressCallback = {}) const;

private:
    bool copyFileThroughBackends(const PreflightItem& item,
                                 const IStorageBackend& sourceBackend,
                                 const IStorageBackend& destinationBackend,
                                 bool createBackup,
                                 QStringList& messages) const;

    bool backupExistingFile(const PreflightItem& item,
                            const IStorageBackend& destinationBackend,
                            QStringList& messages) const;
};

} // namespace stl
