#include "RemoteFileModel.h"
#include "FormatUtils.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

RemoteFileModel::RemoteFileModel(QObject* parent)
    : QStandardItemModel(parent) {
    setHorizontalHeaderLabels({
        QStringLiteral("Имя"),
        QStringLiteral("Тип"),
        QStringLiteral("Размер"),
        QStringLiteral("Изменён")
    });
}

void RemoteFileModel::setBackend(const stl::IStorageBackend* backend) {
    m_backend = backend;
}

const stl::IStorageBackend* RemoteFileModel::backend() const {
    return m_backend;
}

void RemoteFileModel::setCurrentPath(const QString& path) {
    m_currentPath = path.isEmpty() ? QStringLiteral("/") : path;
}

QString RemoteFileModel::currentPath() const {
    return m_currentPath;
}

void RemoteFileModel::setFilterText(const QString& text) {
    m_filterText = text.trimmed();
}

QString RemoteFileModel::filterText() const {
    return m_filterText;
}

void RemoteFileModel::setFilterMode(stl::FileFilterMode mode) {
    m_filterMode = mode;
}

stl::FileFilterMode RemoteFileModel::filterMode() const {
    return m_filterMode;
}

bool RemoteFileModel::reload(QString* error) {
    clear();
    setHorizontalHeaderLabels({
        QStringLiteral("Имя"),
        QStringLiteral("Тип"),
        QStringLiteral("Размер"),
        QStringLiteral("Изменён")
    });

    if (!m_backend) {
        if (error) {
            *error = QStringLiteral("Сервер правой панели не задан");
        }
        return false;
    }

    const QVector<stl::StorageObjectInfo> objects = m_backend->enumerate(m_currentPath, false, false);
    if (objects.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Сервер не вернул объекты для каталога %1").arg(m_currentPath);
        }
        return false;
    }

    const stl::StorageObjectInfo root = objects.first();
    if (!root.valid || !root.exists || !root.isDirectory) {
        if (error) {
            *error = root.error.isEmpty()
                ? QStringLiteral("Текущий путь правой панели не является доступным каталогом: %1").arg(m_currentPath)
                : root.error;
        }
        return false;
    }

    appendParentRow();

    for (int i = 1; i < objects.size(); ++i) {
        const stl::StorageObjectInfo& info = objects[i];
        if (!info.valid || !info.exists || !acceptsObject(info)) {
            continue;
        }

        auto* nameItem = new QStandardItem(displayNameForObject(info));
        auto* typeItem = new QStandardItem(info.isDirectory ? QStringLiteral("Папка") : QStringLiteral("Файл"));
        auto* sizeItem = new QStandardItem(formatSize(info));
        auto* modifiedItem = new QStandardItem(info.modified.isValid()
            ? info.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("-"));

        const QList<QStandardItem*> row = { nameItem, typeItem, sizeItem, modifiedItem };
        for (QStandardItem* item : row) {
            item->setEditable(false);
            item->setData(info.absolutePath, AbsolutePathRole);
            item->setData(info.relativePath, RelativePathRole);
            item->setData(info.isDirectory, IsDirectoryRole);
            item->setData(false, ParentDirectoryRole);
        }
        appendRow(row);
    }

    return true;
}

QString RemoteFileModel::absolutePath(const QModelIndex& index) const {
    if (!index.isValid()) {
        return QString();
    }
    return item(index.row(), 0)->data(AbsolutePathRole).toString();
}

QString RemoteFileModel::relativePath(const QModelIndex& index) const {
    if (!index.isValid()) {
        return QString();
    }
    return item(index.row(), 0)->data(RelativePathRole).toString();
}

bool RemoteFileModel::isDirectory(const QModelIndex& index) const {
    if (!index.isValid()) {
        return false;
    }
    return item(index.row(), 0)->data(IsDirectoryRole).toBool();
}

bool RemoteFileModel::isNavigationUp(const QModelIndex& index) const {
    return isParentDirectoryRow(index);
}

bool RemoteFileModel::isParentDirectoryRow(const QModelIndex& index) const {
    if (!index.isValid()) {
        return false;
    }
    return item(index.row(), 0)->data(ParentDirectoryRole).toBool();
}

QString RemoteFileModel::formatSize(const stl::StorageObjectInfo& info) const {
    if (info.isDirectory) {
        return QStringLiteral("-");
    }

    return ui::formatBytesRu(info.size);
}

QString RemoteFileModel::displayNameForObject(const stl::StorageObjectInfo& info) const {
    const QString name = info.name.isEmpty() ? QFileInfo(info.absolutePath).fileName() : info.name;
    return QStringLiteral("%1 %2").arg(info.isDirectory ? QStringLiteral("📁") : QStringLiteral("📄"), name);
}

QString RemoteFileModel::parentPath() const {
    if (m_currentPath.isEmpty() || m_currentPath == QStringLiteral("/")) {
        return QString();
    }

    if (m_backend && m_backend->isRemote()) {
        QString clean = QDir::cleanPath(m_currentPath);
        clean.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (clean == QStringLiteral(".") || clean == QStringLiteral("/")) {
            return QString();
        }
        while (clean.endsWith(QLatin1Char('/')) && clean.size() > 1) {
            clean.chop(1);
        }
        const int slash = clean.lastIndexOf(QLatin1Char('/'));
        if (slash <= 0) {
            return QStringLiteral("/");
        }
        return clean.left(slash);
    }

    const QString parent = QFileInfo(m_currentPath).absoluteDir().absolutePath();
    if (parent == m_currentPath || parent.isEmpty()) {
        return QString();
    }
    return parent;
}

bool RemoteFileModel::canShowParentRow() const {
    return !parentPath().isEmpty();
}

void RemoteFileModel::appendParentRow() {
    if (!canShowParentRow()) {
        return;
    }

    auto* nameItem = new QStandardItem(QStringLiteral("📁 .."));
    auto* typeItem = new QStandardItem(QStringLiteral("Назад"));
    auto* sizeItem = new QStandardItem(QStringLiteral("-"));
    auto* modifiedItem = new QStandardItem(QStringLiteral("-"));

    const QList<QStandardItem*> row = { nameItem, typeItem, sizeItem, modifiedItem };
    const QString parent = parentPath();
    for (QStandardItem* item : row) {
        item->setEditable(false);
        item->setData(parent, AbsolutePathRole);
        item->setData(parent, RelativePathRole);
        item->setData(true, IsDirectoryRole);
        item->setData(true, ParentDirectoryRole);
    }
    appendRow(row);
}

bool RemoteFileModel::acceptsObject(const stl::StorageObjectInfo& info) const {
    if (m_filterMode == stl::FileFilterMode::FilesOnly && info.isDirectory) {
        return false;
    }
    if (m_filterMode == stl::FileFilterMode::DirectoriesOnly && !info.isDirectory) {
        return false;
    }

    const QString name = info.name.isEmpty() ? QFileInfo(info.absolutePath).fileName() : info.name;
    return nameMatches(name);
}

QStringList RemoteFileModel::filterParts() const {
    return m_filterText.split(QRegularExpression(QStringLiteral("[;\\s]+")), Qt::SkipEmptyParts);
}

bool RemoteFileModel::nameMatches(const QString& name) const {
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

bool RemoteFileModel::wildcardMatches(const QString& name, const QString& pattern) const {
    const QString regexText = QRegularExpression::wildcardToRegularExpression(pattern);
    const QRegularExpression regex(regexText, QRegularExpression::CaseInsensitiveOption);
    return regex.match(name).hasMatch();
}
