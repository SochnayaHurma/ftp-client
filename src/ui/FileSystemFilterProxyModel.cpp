#include "FileSystemFilterProxyModel.h"
#include "FormatUtils.h"

#include <QFileInfo>
#include <QRegularExpression>

FileSystemFilterProxyModel::FileSystemFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent) {
    setRecursiveFilteringEnabled(true);
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
    if (acceptsFileInfo(info, true)) {
        return true;
    }

    // Главное поведение поиска: если совпал вложенный файл, оставляем родительские
    // папки видимыми. Иначе пользователь не сможет добраться до найденного имени.
    if (!m_filterText.isEmpty() && info.isDir()) {
        return childMatchesFilter(sourceIndex);
    }

    return false;
}

bool FileSystemFilterProxyModel::acceptsFileInfo(const QFileInfo& info, bool allowDirectoryNavigation) const {
    if (m_mode == stl::FileFilterMode::FilesOnly && info.isDir()) {
        return allowDirectoryNavigation && !m_filterText.isEmpty() && nameMatches(info.fileName());
    }
    if (m_mode == stl::FileFilterMode::DirectoriesOnly && !info.isDir()) {
        return false;
    }

    return nameMatches(info.fileName());
}

bool FileSystemFilterProxyModel::childMatchesFilter(const QModelIndex& sourceIndex) const {
    const auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
    if (!fsModel) {
        return false;
    }

    const int rows = fsModel->rowCount(sourceIndex);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex child = fsModel->index(row, 0, sourceIndex);
        if (!child.isValid()) {
            continue;
        }
        const QFileInfo childInfo = fsModel->fileInfo(child);
        if (acceptsFileInfo(childInfo, false)) {
            return true;
        }
        if (childInfo.isDir() && childMatchesFilter(child)) {
            return true;
        }
    }
    return false;
}

QStringList FileSystemFilterProxyModel::filterParts() const {
    return m_filterText.split(QRegularExpression(QStringLiteral("[;\\s]+")), Qt::SkipEmptyParts);
}

bool FileSystemFilterProxyModel::nameMatches(const QString& name) const {
    if (m_filterText.isEmpty()) {
        return true;
    }

    const QStringList parts = filterParts();
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

QVariant FileSystemFilterProxyModel::data(const QModelIndex& index, int role) const {
    if (role == Qt::DisplayRole && (index.column() == 1 || index.column() == 2)) {
        const auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
        if (!fsModel) {
            return QSortFilterProxyModel::data(index, role);
        }

        const QModelIndex sourceIndex = mapToSource(index);
        const QFileInfo info = fsModel->fileInfo(sourceIndex);

        if (index.column() == 1) {
            return info.isDir() ? QString() : ui::formatBytesRu(info.size());
        }

        return typeText(info);
    }

    return QSortFilterProxyModel::data(index, role);
}

QString FileSystemFilterProxyModel::typeText(const QFileInfo& info) const {
    if (info.isRoot()) {
        return tr("Диск");
    }
    if (info.isDir()) {
        return tr("Папка");
    }
    return tr("Файл");
}
