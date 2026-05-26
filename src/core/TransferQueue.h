#pragma once

#include "TransferTypes.h"

#include <QString>
#include <QVector>

namespace stl {

enum class QueueItemStatus {
    Waiting,
    Running,
    Done,
    Skipped,
    Error,
    Canceled
};

struct QueueItem {
    int index = -1;
    QString action;
    QString relativePath;
    qint64 bytes = 0;
    QueueItemStatus status = QueueItemStatus::Waiting;
    int progressPercent = 0;
    QString message;
};

inline QString queueStatusToText(QueueItemStatus status) {
    switch (status) {
    case QueueItemStatus::Waiting:
        return QStringLiteral("ожидает");
    case QueueItemStatus::Running:
        return QStringLiteral("выполняется");
    case QueueItemStatus::Done:
        return QStringLiteral("готово");
    case QueueItemStatus::Skipped:
        return QStringLiteral("пропущено");
    case QueueItemStatus::Error:
        return QStringLiteral("ошибка");
    case QueueItemStatus::Canceled:
        return QStringLiteral("отменено");
    }
    return QStringLiteral("неизвестно");
}

inline QString queueActionFromPreflightStatus(PreflightStatus status) {
    switch (status) {
    case PreflightStatus::NewDirectory:
        return QStringLiteral("Создание папки");
    case PreflightStatus::NewFile:
        return QStringLiteral("Копирование нового файла");
    case PreflightStatus::Overwrite:
        return QStringLiteral("Перезапись файла");
    case PreflightStatus::Same:
        return QStringLiteral("Пропуск совпадающего файла");
    case PreflightStatus::Conflict:
        return QStringLiteral("Конфликт");
    case PreflightStatus::Error:
    default:
        return QStringLiteral("Ошибка анализа");
    }
}

inline QVector<QueueItem> buildQueueFromPlan(const QVector<PreflightItem>& plan) {
    QVector<QueueItem> queue;
    queue.reserve(plan.size());

    for (int i = 0; i < plan.size(); ++i) {
        const PreflightItem& item = plan[i];
        QueueItem queueItem;
        queueItem.index = i;
        queueItem.action = queueActionFromPreflightStatus(item.status);
        queueItem.relativePath = item.relativePath;
        queueItem.bytes = item.isDirectory ? 0 : item.sourceSize;
        queueItem.status = QueueItemStatus::Waiting;
        queueItem.progressPercent = 0;
        queueItem.message = item.warning;
        queue.push_back(queueItem);
    }

    return queue;
}

} 
