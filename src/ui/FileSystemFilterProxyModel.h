#pragma once

#include "FileFilterMode.h"

#include <QFileInfo>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

class FileSystemFilterProxyModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit FileSystemFilterProxyModel(QObject* parent = nullptr);

    void setFilterText(const QString& text);
    QString filterText() const;

    void setFilterMode(stl::FileFilterMode mode);
    stl::FileFilterMode filterMode() const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    bool acceptsFileInfo(const QFileInfo& info, bool allowDirectoryNavigation) const;
    bool childMatchesFilter(const QModelIndex& sourceIndex) const;
    bool nameMatches(const QString& name) const;
    bool wildcardMatches(const QString& name, const QString& pattern) const;
    QStringList filterParts() const;
    QString typeText(const QFileInfo& info) const;

    QString m_filterText;
    stl::FileFilterMode m_mode = stl::FileFilterMode::All;
};
