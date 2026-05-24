#include "Journal.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace stl {

Journal::Journal(const QString& databasePath)
    : m_databasePath(databasePath) {
}

QString Journal::connectionName() const {
    return QStringLiteral("secure_transfer_lite_journal");
}

bool Journal::open() {
    QSqlDatabase db;

    if (QSqlDatabase::contains(connectionName())) {
        db = QSqlDatabase::database(connectionName());
    } else {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName());
        db.setDatabaseName(m_databasePath);
    }

    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    const QString operationsSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS operations ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "created_at TEXT NOT NULL, "
        "direction TEXT NOT NULL, "
        "files_total INTEGER NOT NULL, "
        "copied INTEGER NOT NULL, "
        "skipped INTEGER NOT NULL, "
        "errors INTEGER NOT NULL, "
        "overwrite_count INTEGER NOT NULL"
        ")");

    if (!query.exec(operationsSql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    const QString itemsSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS operation_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "operation_id INTEGER NOT NULL, "
        "relative_path TEXT NOT NULL, "
        "source_path TEXT NOT NULL, "
        "destination_path TEXT NOT NULL, "
        "status TEXT NOT NULL, "
        "risk TEXT NOT NULL, "
        "source_sha256 TEXT, "
        "destination_sha256 TEXT, "
        "warning TEXT, "
        "FOREIGN KEY(operation_id) REFERENCES operations(id)"
        ")");

    if (!query.exec(itemsSql)) {
        m_lastError = query.lastError().text();
        return false;
    }

    return true;
}

bool Journal::logOperation(TransferDirection direction,
                           const QVector<PreflightItem>& plan,
                           int copied,
                           int skipped,
                           int errors) {
    QSqlDatabase db = QSqlDatabase::database(connectionName());

    if (!db.isOpen() && !open()) {
        return false;
    }

    int overwriteCount = 0;
    for (const PreflightItem& item : plan) {
        if (item.status == PreflightStatus::Overwrite) {
            overwriteCount++;
        }
    }

    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }

    QSqlQuery operationQuery(db);
    operationQuery.prepare(QStringLiteral(
        "INSERT INTO operations "
        "(created_at, direction, files_total, copied, skipped, errors, overwrite_count) "
        "VALUES (:created_at, :direction, :files_total, :copied, :skipped, :errors, :overwrite_count)"));
    operationQuery.bindValue(QStringLiteral(":created_at"), QDateTime::currentDateTime().toString(Qt::ISODate));
    operationQuery.bindValue(QStringLiteral(":direction"), directionToText(direction));
    operationQuery.bindValue(QStringLiteral(":files_total"), plan.size());
    operationQuery.bindValue(QStringLiteral(":copied"), copied);
    operationQuery.bindValue(QStringLiteral(":skipped"), skipped);
    operationQuery.bindValue(QStringLiteral(":errors"), errors);
    operationQuery.bindValue(QStringLiteral(":overwrite_count"), overwriteCount);

    if (!operationQuery.exec()) {
        m_lastError = operationQuery.lastError().text();
        db.rollback();
        return false;
    }

    const qint64 operationId = operationQuery.lastInsertId().toLongLong();

    QSqlQuery itemQuery(db);
    itemQuery.prepare(QStringLiteral(
        "INSERT INTO operation_items "
        "(operation_id, relative_path, source_path, destination_path, status, risk, source_sha256, destination_sha256, warning) "
        "VALUES (:operation_id, :relative_path, :source_path, :destination_path, :status, :risk, :source_sha256, :destination_sha256, :warning)"));

    for (const PreflightItem& item : plan) {
        itemQuery.bindValue(QStringLiteral(":operation_id"), operationId);
        itemQuery.bindValue(QStringLiteral(":relative_path"), item.relativePath);
        itemQuery.bindValue(QStringLiteral(":source_path"), item.sourcePath);
        itemQuery.bindValue(QStringLiteral(":destination_path"), item.destinationPath);
        itemQuery.bindValue(QStringLiteral(":status"), statusToText(item.status));
        itemQuery.bindValue(QStringLiteral(":risk"), riskToText(item.risk));
        itemQuery.bindValue(QStringLiteral(":source_sha256"), item.sourceSha256);
        itemQuery.bindValue(QStringLiteral(":destination_sha256"), item.destinationSha256);
        itemQuery.bindValue(QStringLiteral(":warning"), item.warning);

        if (!itemQuery.exec()) {
            m_lastError = itemQuery.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        m_lastError = db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

QVector<OperationSummary> Journal::recentOperations(int limit) {
    QVector<OperationSummary> result;
    QSqlDatabase db = QSqlDatabase::database(connectionName());

    if (!db.isOpen() && !open()) {
        return result;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, created_at, direction, files_total, copied, skipped, errors, overwrite_count "
        "FROM operations ORDER BY id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), limit <= 0 ? 100 : limit);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return result;
    }

    while (query.next()) {
        OperationSummary item;
        item.id = query.value(0).toLongLong();
        item.createdAt = query.value(1).toString();
        item.direction = query.value(2).toString();
        item.filesTotal = query.value(3).toInt();
        item.copied = query.value(4).toInt();
        item.skipped = query.value(5).toInt();
        item.errors = query.value(6).toInt();
        item.overwriteCount = query.value(7).toInt();
        result.push_back(item);
    }

    return result;
}

QVector<OperationItemRecord> Journal::operationItems(qint64 operationId) {
    QVector<OperationItemRecord> result;
    QSqlDatabase db = QSqlDatabase::database(connectionName());

    if (!db.isOpen() && !open()) {
        return result;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT id, operation_id, relative_path, source_path, destination_path, status, risk, "
        "source_sha256, destination_sha256, warning "
        "FROM operation_items WHERE operation_id = :operation_id ORDER BY id ASC"));
    query.bindValue(QStringLiteral(":operation_id"), operationId);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return result;
    }

    while (query.next()) {
        OperationItemRecord item;
        item.id = query.value(0).toLongLong();
        item.operationId = query.value(1).toLongLong();
        item.relativePath = query.value(2).toString();
        item.sourcePath = query.value(3).toString();
        item.destinationPath = query.value(4).toString();
        item.status = query.value(5).toString();
        item.risk = query.value(6).toString();
        item.sourceSha256 = query.value(7).toString();
        item.destinationSha256 = query.value(8).toString();
        item.warning = query.value(9).toString();
        result.push_back(item);
    }

    return result;
}

QString Journal::lastError() const {
    return m_lastError;
}

QString Journal::databasePath() const {
    return m_databasePath;
}

} // namespace stl
