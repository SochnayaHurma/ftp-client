#pragma once

#include "../core/storage/IStorageBackend.h"
#include "FileFilterMode.h"

#include <QStandardItemModel>
#include <QString>
#include <QStringList>

class RemoteFileModel final : public QStandardItemModel {
    Q_OBJECT

public:
    enum CustomRole {
        AbsolutePathRole = Qt::UserRole + 1,
        RelativePathRole,
        IsDirectoryRole,
        ParentDirectoryRole
    };

    explicit RemoteFileModel(QObject* parent = nullptr);

    void setBackend(const stl::IStorageBackend* backend);
    const stl::IStorageBackend* backend() const;

    void setCurrentPath(const QString& path);
    QString currentPath() const;

    void setFilterText(const QString& text);
    QString filterText() const;

    void setFilterMode(stl::FileFilterMode mode);
    stl::FileFilterMode filterMode() const;

    bool reload(QString* error = nullptr);
    QString absolutePath(const QModelIndex& index) const;
    QString relativePath(const QModelIndex& index) const;
    bool isDirectory(const QModelIndex& index) const;
    bool isNavigationUp(const QModelIndex& index) const;
    bool isParentDirectoryRow(const QModelIndex& index) const;

private:
    QString formatSize(const stl::StorageObjectInfo& info) const;
    QString displayNameForObject(const stl::StorageObjectInfo& info) const;
    QString parentPath() const;
    bool canShowParentRow() const;
    void appendParentRow();
    bool acceptsObject(const stl::StorageObjectInfo& info) const;
    bool nameMatches(const QString& name) const;
    bool wildcardMatches(const QString& name, const QString& pattern) const;
    QStringList filterParts() const;

    const stl::IStorageBackend* m_backend = nullptr;
    QString m_currentPath = QStringLiteral("/");
    QString m_filterText;
    stl::FileFilterMode m_filterMode = stl::FileFilterMode::All;
};
