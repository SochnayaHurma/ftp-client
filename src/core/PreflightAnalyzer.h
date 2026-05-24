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
                                   const QString& destinationRoot) const;

    QVector<PreflightItem> analyze(const QStringList& selectedPaths,
                                   const IStorageBackend& sourceBackend,
                                   const IStorageBackend& destinationBackend) const;

private:
    void analyzePath(const QString& sourcePath,
                     const IStorageBackend& sourceBackend,
                     const IStorageBackend& destinationBackend,
                     QVector<PreflightItem>& result) const;

    PreflightItem makeItem(const StorageObjectInfo& sourceInfo,
                           const IStorageBackend& destinationBackend) const;

    RiskLevel riskForOverwrite(const StorageObjectInfo& sourceInfo,
                               const StorageObjectInfo& destinationInfo) const;
};

} // namespace stl
