#pragma once

#include "TransferTypes.h"
#include "storage/IStorageBackend.h"

#include <QVector>
#include <QStringList>

namespace stl {

class PreflightAnalyzer {
public:
    QVector<PreflightItem> analyze(const QStringList& selectedPaths,
                                   const QString& sourceRoot,
                                   const QString& destinationRoot,
                                   bool preserveSourceHierarchy = true) const;

    QVector<PreflightItem> analyze(const QStringList& selectedPaths,
                                   const IStorageBackend& sourceBackend,
                                   const IStorageBackend& destinationBackend,
                                   bool preserveSourceHierarchy = true) const;

private:
    void analyzePath(const QString& sourcePath,
                     const IStorageBackend& sourceBackend,
                     const IStorageBackend& destinationBackend,
                     bool preserveSourceHierarchy,
                     QVector<PreflightItem>& result) const;
    QVector<StorageObjectInfo> rebaseSelectedObjects(const QString& sourcePath,
                                                     const QVector<StorageObjectInfo>& objects) const;

    PreflightItem makeItem(const StorageObjectInfo& sourceInfo,
                           const IStorageBackend& destinationBackend) const;

    RiskLevel riskForOverwrite(const StorageObjectInfo& sourceInfo,
                               const StorageObjectInfo& destinationInfo) const;
};

} // namespace stl
