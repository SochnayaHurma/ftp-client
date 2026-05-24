#include "FileSystemFilterProxyModel.h"
#include "FormatUtils.h"

#include <QFileInfo>
#include <QRegularExpression>

FileSystemFilterProxyModel::FileSystemFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent) {
    setRecursiveFilteringEnabled(false);
    setDynamicSortFilter(true);
}

void FileSystemFilterProxyModel::setFilterText(const QString& text) {
    const QString normalized = text.trimmed();
    if (m_filterText == normalized) {
        return;
    }
    m_filterText = normalized;
    invalidateFilter();
}

QString FileSystemFilterProxyModel::filterText() const {
    return m_filterText;
}

void FileSystemFilterProxyModel::setFilterMode(stl::FileFilterMode mode) {
    if (m_mode == mode) {
        return;
    }
    m_mode = mode;
    invalidateFilter();
}

stl::FileFilterMode FileSystemFilterProxyModel::filterMode() const {
    return m_mode;
}

bool FileSystemFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
    if (!fsModel) {
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

    const QModelIndex sourceIndex = fsModel->index(sourceRow, 0, sourceParent);
    if (!sourceIndex.isValid()) {
        return false;
    }

    const QFileInfo info = fsModel->fileInfo(sourceIndex);
    if (m_mode == stl::FileFilterMode::FilesOnly && info.isDir()) {
        return false;
    }
    if (m_mode == stl::FileFilterMode::DirectoriesOnly && !info.isDir()) {
        return false;
    }

    return nameMatches(info.fileName());
}

bool FileSystemFilterProxyModel::nameMatches(const QString& name) const {
    if (m_filterText.isEmpty()) {
        return true;
    }

    const QStringList parts = m_filterText.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (QString part : parts) {
        part = part.trimmed();
        if (part.isEmpty()) {
            continue;
        }

        if (part.contains(QLatin1Char('*')) || part.contains(QLatin1Char('?'))) {
            if (wildcardMatches(name, part)) {
                return true;
            }
        } else if (name.contains(part, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

bool FileSystemFilterProxyModel::wildcardMatches(const QString& name, const QString& pattern) const {
    const QString regexText = QRegularExpression::wildcardToRegularExpression(pattern);
    const QRegularExpression regex(regexText, QRegularExpression::CaseInsensitiveOption);
    return regex.match(name).hasMatch();
}

QVariant FileSystemFilterProxyModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return tr("Имя");
        case 1: return tr("Размер");
        case 2: return tr("Тип");
        case 3: return tr("Дата изменения");
        }
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

QVariant FileSystemFilterProxyModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole && index.column() == 1) {
        const auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
        if (!fsModel) {
            return QSortFilterProxyModel::data(index, role);
        }

        const QModelIndex sourceIndex = mapToSource(index);
        const QFileInfo info = fsModel->fileInfo(sourceIndex);

        if (info.isDir()) {
            return QString();
        }

        return ui::formatBytesRu(info.size());
    }

    return QSortFilterProxyModel::data(index, role);
}
