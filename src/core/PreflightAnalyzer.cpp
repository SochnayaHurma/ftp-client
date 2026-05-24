#include "PreflightAnalyzer.h"
#include "storage/LocalStorageBackend.h"

#include <QtGlobal>

namespace stl {

QVector<PreflightItem> PreflightAnalyzer::analyze(const QStringList& selectedPaths,
                                                  const QString& sourceRoot,
                                                  const QString& destinationRoot) const {
    LocalStorageBackend sourceBackend(sourceRoot);
    LocalStorageBackend destinationBackend(destinationRoot);
    return analyze(selectedPaths, sourceBackend, destinationBackend);
}

QVector<PreflightItem> PreflightAnalyzer::analyze(const QStringList& selectedPaths,
                                                  const IStorageBackend& sourceBackend,
                                                  const IStorageBackend& destinationBackend) const {
    QVector<PreflightItem> result;

    for (const QString& path : selectedPaths) {
        analyzePath(path, sourceBackend, destinationBackend, result);
    }

    return result;
}

void PreflightAnalyzer::analyzePath(const QString& sourcePath,
                                    const IStorageBackend& sourceBackend,
                                    const IStorageBackend& destinationBackend,
                                    QVector<PreflightItem>& result) const {
    const QVector<StorageObjectInfo> objects = sourceBackend.enumerate(sourcePath, true, true);

    if (objects.isEmpty()) {
        PreflightItem item;
        item.sourcePath = sourcePath;
        item.status = PreflightStatus::Error;
        item.risk = RiskLevel::High;
        item.warning = QStringLiteral("Источник не найден или backend не вернул сведения об объекте");
        result.push_back(item);
        return;
    }

    for (const StorageObjectInfo& object : objects) {
        if (!object.valid) {
            PreflightItem item;
            item.sourcePath = object.absolutePath.isEmpty() ? sourcePath : object.absolutePath;
            item.relativePath = object.relativePath;
            item.status = PreflightStatus::Error;
            item.risk = RiskLevel::High;
            item.warning = object.error.isEmpty()
                ? QStringLiteral("Не удалось получить сведения об объекте")
                : object.error;
            result.push_back(item);
            continue;
        }

        if (!object.exists) {
            PreflightItem item;
            item.sourcePath = object.absolutePath.isEmpty() ? sourcePath : object.absolutePath;
            item.relativePath = object.relativePath;
            item.status = PreflightStatus::Error;
            item.risk = RiskLevel::High;
            item.warning = QStringLiteral("Источник не найден");
            result.push_back(item);
            continue;
        }

        result.push_back(makeItem(object, destinationBackend));
    }
}

PreflightItem PreflightAnalyzer::makeItem(const StorageObjectInfo& sourceInfo,
                                          const IStorageBackend& destinationBackend) const {
    const QString destinationPath = destinationBackend.absolutePath(sourceInfo.relativePath);
    const StorageObjectInfo destinationInfo = destinationBackend.objectInfo(destinationPath, !sourceInfo.isDirectory);

    PreflightItem item;
    item.sourcePath = sourceInfo.absolutePath;
    item.destinationPath = destinationPath;
    item.relativePath = sourceInfo.relativePath;
    item.isDirectory = sourceInfo.isDirectory;
    item.sourceSize = sourceInfo.isDirectory ? 0 : sourceInfo.size;
    item.sourceModified = sourceInfo.modified;
    item.sourceSha256 = sourceInfo.sha256;

    if (!destinationInfo.valid && !destinationInfo.error.isEmpty()) {
        item.status = PreflightStatus::Error;
        item.risk = RiskLevel::High;
        item.warning = destinationInfo.error;
        return item;
    }

    if (!destinationInfo.exists) {
        item.status = sourceInfo.isDirectory ? PreflightStatus::NewDirectory : PreflightStatus::NewFile;
        item.risk = RiskLevel::Low;
        return item;
    }

    item.destinationSize = destinationInfo.isDirectory ? 0 : destinationInfo.size;
    item.destinationModified = destinationInfo.modified;
    item.destinationSha256 = destinationInfo.sha256;

    if (sourceInfo.isDirectory) {
        item.status = destinationInfo.isDirectory ? PreflightStatus::Same : PreflightStatus::Conflict;
        item.risk = item.status == PreflightStatus::Conflict ? RiskLevel::High : RiskLevel::None;
        item.warning = item.status == PreflightStatus::Conflict
            ? QStringLiteral("На месте папки назначения уже существует файл")
            : QString();
        return item;
    }

    if (destinationInfo.isDirectory) {
        item.status = PreflightStatus::Conflict;
        item.risk = RiskLevel::High;
        item.warning = QStringLiteral("На месте файла назначения уже существует папка");
        return item;
    }

    if (item.sourceSha256.isEmpty()) {
        item.status = PreflightStatus::Error;
        item.risk = RiskLevel::High;
        item.warning = QStringLiteral("Не удалось вычислить SHA-256 источника");
        return item;
    }

    if (item.destinationSha256.isEmpty()) {
        item.status = PreflightStatus::Error;
        item.risk = RiskLevel::High;
        item.warning = QStringLiteral("Не удалось вычислить SHA-256 файла назначения");
        return item;
    }

    if (item.sourceSha256 == item.destinationSha256) {
        item.status = PreflightStatus::Same;
        item.risk = RiskLevel::None;
        item.warning = QStringLiteral("Содержимое совпадает по SHA-256");
    } else {
        item.status = PreflightStatus::Overwrite;
        item.risk = riskForOverwrite(sourceInfo, destinationInfo);
        item.warning = QStringLiteral("SHA-256 отличается; файл назначения будет перезаписан, перед операцией может быть создана резервная копия");
    }

    return item;
}

RiskLevel PreflightAnalyzer::riskForOverwrite(const StorageObjectInfo& sourceInfo,
                                              const StorageObjectInfo& destinationInfo) const {
    const qint64 sizeDelta = qAbs(sourceInfo.size - destinationInfo.size);
    const bool destinationNewer = destinationInfo.modified > sourceInfo.modified;

    if (destinationNewer) {
        return RiskLevel::High;
    }

    if (sizeDelta > 1024 * 1024) {
        return RiskLevel::Medium;
    }

    return RiskLevel::Low;
}

} // namespace stl
