#pragma once

#include <QDateTime>
#include <QString>

namespace stl {

enum class TransferDirection {
    Upload,
    Download
};

enum class PreflightStatus {
    NewDirectory,
    NewFile,
    Overwrite,
    Same,
    Conflict,
    Error
};

enum class RiskLevel {
    None,
    Low,
    Medium,
    High
};

struct PreflightItem {
    QString sourcePath;
    QString destinationPath;
    QString relativePath;
    bool isDirectory = false;
    qint64 sourceSize = 0;
    qint64 destinationSize = -1;
    QDateTime sourceModified;
    QDateTime destinationModified;
    QString sourceSha256;
    QString destinationSha256;
    PreflightStatus status = PreflightStatus::Error;
    RiskLevel risk = RiskLevel::None;
    QString warning;
};

inline QString statusToText(PreflightStatus status) {
    switch (status) {
    case PreflightStatus::NewDirectory:
        return QStringLiteral("Новая папка");
    case PreflightStatus::NewFile:
        return QStringLiteral("Новый файл");
    case PreflightStatus::Overwrite:
        return QStringLiteral("Перезапись");
    case PreflightStatus::Same:
        return QStringLiteral("Без изменений");
    case PreflightStatus::Conflict:
        return QStringLiteral("Конфликт");
    case PreflightStatus::Error:
    default:
        return QStringLiteral("Ошибка");
    }
}

inline QString riskToText(RiskLevel risk) {
    switch (risk) {
    case RiskLevel::None:
        return QStringLiteral("нет");
    case RiskLevel::Low:
        return QStringLiteral("низкий");
    case RiskLevel::Medium:
        return QStringLiteral("средний");
    case RiskLevel::High:
        return QStringLiteral("высокий");
    default:
        return QStringLiteral("неизвестно");
    }
}

inline QString directionToText(TransferDirection direction) {
    return direction == TransferDirection::Upload
        ? QStringLiteral("Загрузка на сервер")
        : QStringLiteral("Скачивание с сервера");
}

} 
