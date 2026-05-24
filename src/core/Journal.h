#pragma once

#include "TransferTypes.h"

#include <QString>
#include <QVector>

namespace stl {

struct OperationSummary {
    qint64 id = -1;
    QString createdAt;
    QString direction;
    int filesTotal = 0;
    int copied = 0;
    int skipped = 0;
    int errors = 0;
    int overwriteCount = 0;
};

struct OperationItemRecord {
    qint64 id = -1;
    qint64 operationId = -1;
    QString relativePath;
    QString sourcePath;
    QString destinationPath;
    QString status;
    QString risk;
    QString sourceSha256;
    QString destinationSha256;
    QString warning;
};

class Journal {
public:
    explicit Journal(const QString& databasePath);

    bool open();
    bool logOperation(TransferDirection direction,
                      const QVector<PreflightItem>& plan,
                      int copied,
                      int skipped,
                      int errors);

    QVector<OperationSummary> recentOperations(int limit = 100);
    QVector<OperationItemRecord> operationItems(qint64 operationId);

    QString lastError() const;
    QString databasePath() const;

private:
    QString connectionName() const;
    QString m_databasePath;
    QString m_lastError;
};

} // namespace stl
