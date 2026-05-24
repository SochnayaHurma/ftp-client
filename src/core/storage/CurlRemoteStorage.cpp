#include "CurlRemoteStorage.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QUrl>

#ifdef SECURETRANSFER_WITH_CURL
#include <curl/curl.h>
#endif

namespace stl {

namespace {

#ifdef SECURETRANSFER_WITH_CURL

size_t writeToByteArray(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buffer = static_cast<QByteArray*>(userdata);
    const size_t bytes = size * nmemb;
    buffer->append(ptr, static_cast<qsizetype>(bytes));
    return bytes;
}

size_t writeToQFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* file = static_cast<QFile*>(userdata);
    const size_t bytes = size * nmemb;
    const qint64 written = file->write(ptr, static_cast<qint64>(bytes));
    return written < 0 ? 0 : static_cast<size_t>(written);
}

size_t readFromQFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* file = static_cast<QFile*>(userdata);
    const size_t capacity = size * nmemb;
    const qint64 read = file->read(ptr, static_cast<qint64>(capacity));
    return read < 0 ? 0 : static_cast<size_t>(read);
}

QString curlCodeToText(CURLcode code) {
    return QString::fromUtf8(curl_easy_strerror(code));
}

bool ensureCurlGlobalInit() {
    static const bool initialized = (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    return initialized;
}

void configureCommonOptions(CURL* curl,
                            const RemoteConnectionProfile& profile,
                            const QString& url,
                            char* errorBuffer) {
    const QByteArray urlBytes = url.toUtf8();
    const QByteArray userBytes = profile.username.toUtf8();
    const QByteArray passBytes = profile.password.toUtf8();

    curl_easy_setopt(curl, CURLOPT_URL, urlBytes.constData());
    curl_easy_setopt(curl, CURLOPT_USERNAME, userBytes.constData());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, passBytes.constData());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, profile.timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, profile.timeoutSeconds <= 0 ? 30 : profile.timeoutSeconds * 4);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

    if (profile.kind == StorageKind::Ftp) {
        curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, profile.passiveMode ? 1L : 0L);
        if (!profile.passiveMode) {
            curl_easy_setopt(curl, CURLOPT_FTPPORT, "-");
        }
    }
}

QString errorFromBufferOrCode(const char* buffer, CURLcode code) {
    const QString detailed = QString::fromUtf8(buffer).trimmed();
    return detailed.isEmpty() ? curlCodeToText(code) : detailed;
}

struct DirectoryListEntry {
    QString name;
    bool isDirectory = false;
    qint64 size = -1;
    bool hasType = false;
};

DirectoryListEntry parseDirectoryListLine(QString line, bool dirListOnlyMode) {
    DirectoryListEntry entry;
    line = line.trimmed();

    if (line.isEmpty() || line == QStringLiteral(".") || line == QStringLiteral("..")) {
        return entry;
    }

    if (dirListOnlyMode) {
        entry.name = line;
        if (entry.name.endsWith(QLatin1Char('/'))) {
            entry.name.chop(1);
            entry.isDirectory = true;
            entry.hasType = true;
        }
        return entry;
    }

    // UNIX LIST example:
    // drwxr-xr-x  2 user group 4096 Jan 01 12:00 public_html
    // -rw-r--r--  1 user group  123 Jan 01 12:00 index.html
    static const QRegularExpression unixList(
        QStringLiteral("^([dl-])[rwxstST-]{9}\\s+\\S+\\s+\\S+\\s+\\S+\\s+(\\d+)\\s+\\S+\\s+\\d+\\s+(?:[\\d:]+|\\d{4})\\s+(.+)$"));
    QRegularExpressionMatch match = unixList.match(line);
    if (match.hasMatch()) {
        QString name = match.captured(3).trimmed();
        const int symlinkPos = name.indexOf(QStringLiteral(" -> "));
        if (symlinkPos >= 0) {
            name = name.left(symlinkPos).trimmed();
        }
        entry.name = name;
        entry.isDirectory = match.captured(1) == QStringLiteral("d");
        entry.size = entry.isDirectory ? 0 : match.captured(2).toLongLong();
        entry.hasType = true;
        return entry;
    }

    // Windows/IIS LIST example:
    // 01-01-24  12:00PM       <DIR>          public_html
    // 01-01-24  12:00PM                 123 index.html
    static const QRegularExpression windowsList(
        QStringLiteral("^\\d{2}-\\d{2}-\\d{2}\\s+\\d{2}:\\d{2}[AP]M\\s+(<DIR>|\\d+)\\s+(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    match = windowsList.match(line);
    if (match.hasMatch()) {
        const QString sizeOrDir = match.captured(1);
        entry.name = match.captured(2).trimmed();
        entry.isDirectory = sizeOrDir.compare(QStringLiteral("<DIR>"), Qt::CaseInsensitive) == 0;
        entry.size = entry.isDirectory ? 0 : sizeOrDir.toLongLong();
        entry.hasType = true;
        return entry;
    }

    // Fallback: some servers ignore LIST format expectations and return plain names.
    entry.name = line;
    return entry;
}

bool performDirectoryList(const RemoteConnectionProfile& profile,
                          const QString& url,
                          bool dirListOnlyMode,
                          QByteArray* listing,
                          QString* error) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = QStringLiteral("Не удалось инициализировать libcurl");
        }
        return false;
    }

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    configureCommonOptions(curl, profile, url, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, dirListOnlyMode ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToByteArray);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, listing);

    const CURLcode code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (code == CURLE_OK) {
        return true;
    }

    if (error) {
        *error = errorFromBufferOrCode(errorBuffer, code);
    }
    return false;
}

#endif

}

CurlRemoteStorage::CurlRemoteStorage() {
}

CurlRemoteStorage::CurlRemoteStorage(const RemoteConnectionProfile& profile)
    : m_profile(profile) {
    setRoot(profile.remoteRoot);
}

bool CurlRemoteStorage::isBuiltWithCurl() {
#ifdef SECURETRANSFER_WITH_CURL
    return true;
#else
    return false;
#endif
}

void CurlRemoteStorage::setProfile(const RemoteConnectionProfile& profile) {
    m_profile = profile;
    setRoot(profile.remoteRoot);
}

RemoteConnectionProfile CurlRemoteStorage::profile() const {
    return m_profile;
}

QString CurlRemoteStorage::backendName() const {
#ifdef SECURETRANSFER_WITH_CURL
    return QStringLiteral("CurlRemoteStorage");
#else
    return QStringLiteral("CurlRemoteStorage (libcurl не подключён)");
#endif
}

StorageKind CurlRemoteStorage::kind() const {
    return m_profile.kind;
}

bool CurlRemoteStorage::isRemote() const {
    return true;
}

QString CurlRemoteStorage::root() const {
    return m_root;
}

void CurlRemoteStorage::setRoot(const QString& rootPath) {
    m_root = normalizeRemotePath(rootPath.isEmpty() ? QStringLiteral("/") : rootPath);
}

QString CurlRemoteStorage::absolutePath(const QString& relativePath) const {
    QString rel = relativePath;
    while (rel.startsWith(QLatin1Char('/'))) {
        rel.remove(0, 1);
    }

    if (rel.isEmpty() || rel == QStringLiteral(".")) {
        return m_root;
    }

    return normalizeRemotePath(m_root + QLatin1Char('/') + rel);
}

QString CurlRemoteStorage::relativePath(const QString& absolutePath) const {
    const QString normalized = normalizeRemotePath(absolutePath);
    QString normalizedRoot = normalizeRemotePath(m_root);
    if (!normalizedRoot.endsWith(QLatin1Char('/'))) {
        normalizedRoot.append(QLatin1Char('/'));
    }

    if (normalized == normalizeRemotePath(m_root)) {
        return QStringLiteral(".");
    }

    if (normalized.startsWith(normalizedRoot)) {
        return normalized.mid(normalizedRoot.size());
    }

    return QStringLiteral("../") + normalized;
}

StorageObjectInfo CurlRemoteStorage::unavailableInfo(const QString& path, const QString& message) const {
    StorageObjectInfo result;
    result.valid = false;
    result.exists = false;
    result.absolutePath = normalizeRemotePath(path);
    result.relativePath = relativePath(result.absolutePath);
    result.name = QFileInfo(result.absolutePath).fileName();
    result.error = message;
    return result;
}

bool CurlRemoteStorage::testConnection(QString* error) const {
    QString validationError;
    if (!m_profile.isValid(&validationError)) {
        if (error) {
            *error = validationError;
        }
        return false;
    }

#ifndef SECURETRANSFER_WITH_CURL
    if (error) {
        *error = QStringLiteral("Проект собран без libcurl. Пересоберите с -DSECURETRANSFER_WITH_CURL=ON.");
    }
    return false;
#else
    const QVector<StorageObjectInfo> objects = enumerate(m_root, false, false);
    if (!objects.isEmpty() && objects.first().valid && objects.first().exists) {
        return true;
    }

    if (error) {
        if (!objects.isEmpty() && !objects.first().error.isEmpty()) {
            *error = objects.first().error;
        } else {
            *error = QStringLiteral("Не удалось получить список удалённого каталога");
        }
    }
    return false;
#endif
}

StorageObjectInfo CurlRemoteStorage::objectInfo(const QString& absolutePath, bool calculateHash) const {
    const QString remotePath = normalizeRemotePath(absolutePath);

#ifndef SECURETRANSFER_WITH_CURL
    Q_UNUSED(calculateHash)
    return unavailableInfo(remotePath, QStringLiteral("CurlRemoteStorage недоступен: проект собран без libcurl"));
#else
    QString validationError;
    if (!m_profile.isValid(&validationError)) {
        return unavailableInfo(remotePath, validationError);
    }

    if (!ensureCurlGlobalInit()) {
        return unavailableInfo(remotePath, QStringLiteral("Не удалось выполнить глобальную инициализацию libcurl"));
    }

    StorageObjectInfo result;
    result.valid = true;
    result.exists = false;
    result.absolutePath = remotePath;
    result.relativePath = relativePath(remotePath);
    result.name = QFileInfo(remotePath).fileName();
    if (result.name.isEmpty()) {
        result.name = QStringLiteral("/");
    }

    {
        CURL* curl = curl_easy_init();
        if (!curl) {
            return unavailableInfo(remotePath, QStringLiteral("Не удалось инициализировать libcurl"));
        }

        char errorBuffer[CURL_ERROR_SIZE] = {0};
        configureCommonOptions(curl, m_profile, makeUrl(remotePath, false), errorBuffer);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);

        const CURLcode code = curl_easy_perform(curl);
        if (code == CURLE_OK) {
            curl_off_t length = -1;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length);
            curl_off_t filetime = -1;
            curl_easy_getinfo(curl, CURLINFO_FILETIME_T, &filetime);

            result.exists = true;
            result.isDirectory = false;
            result.size = length < 0 ? 0 : static_cast<qint64>(length);
            if (filetime > 0) {
                result.modified = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(filetime));
            }
            curl_easy_cleanup(curl);

            if (calculateHash) {
                QTemporaryDir tempDir;
                if (!tempDir.isValid()) {
                    result.valid = false;
                    result.error = QStringLiteral("Не удалось создать временный каталог для SHA-256 удалённого файла");
                    return result;
                }
                const QString tempFile = tempDir.filePath(QStringLiteral("remote_hash_payload.bin"));
                QString readError;
                if (!readFileToLocal(remotePath, tempFile, &readError)) {
                    result.valid = false;
                    result.error = readError;
                    return result;
                }
                result.sha256 = fileSha256(tempFile);
                if (result.sha256.isEmpty()) {
                    result.valid = false;
                    result.error = QStringLiteral("Не удалось вычислить SHA-256 удалённого файла");
                }
            }

            return result;
        }
        curl_easy_cleanup(curl);
    }

    QByteArray listing;
    CURL* curl = curl_easy_init();
    if (!curl) {
        return unavailableInfo(remotePath, QStringLiteral("Не удалось инициализировать libcurl"));
    }

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    configureCommonOptions(curl, m_profile, makeUrl(remotePath, true), errorBuffer);
    curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToByteArray);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &listing);

    const CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_OK) {
        result.exists = true;
        result.isDirectory = true;
        result.size = 0;
        curl_easy_cleanup(curl);
        return result;
    }

    result.exists = false;
    result.error = errorFromBufferOrCode(errorBuffer, code);
    curl_easy_cleanup(curl);
    return result;
#endif
}

QVector<StorageObjectInfo> CurlRemoteStorage::enumerate(const QString& absolutePath,
                                                        bool recursive,
                                                        bool calculateHash) const {
    const QString remotePath = normalizeRemotePath(absolutePath);
    QVector<StorageObjectInfo> result;

#ifndef SECURETRANSFER_WITH_CURL
    Q_UNUSED(recursive)
    Q_UNUSED(calculateHash)
    result.push_back(unavailableInfo(remotePath, QStringLiteral("CurlRemoteStorage недоступен: проект собран без libcurl")));
    return result;
#else
    StorageObjectInfo rootInfo;
    rootInfo.valid = true;
    rootInfo.exists = true;
    rootInfo.isDirectory = true;
    rootInfo.absolutePath = remotePath;
    rootInfo.relativePath = relativePath(remotePath);
    rootInfo.name = QFileInfo(remotePath).fileName();
    if (rootInfo.name.isEmpty()) {
        rootInfo.name = QStringLiteral("/");
    }
    rootInfo.size = 0;
    result.push_back(rootInfo);

    QString validationError;
    if (!m_profile.isValid(&validationError)) {
        result[0].valid = false;
        result[0].exists = false;
        result[0].error = validationError;
        return result;
    }

    if (!ensureCurlGlobalInit()) {
        result[0].valid = false;
        result[0].exists = false;
        result[0].error = QStringLiteral("Не удалось выполнить глобальную инициализацию libcurl");
        return result;
    }

    // Важно: для реальных FTP серверов нельзя сначала доверять objectInfo()/NOBODY
    // при проверке каталога. Некоторые серверы успешно принимают upload, но неверно
    // отвечают на SIZE/HEAD для папки. Поэтому для текущей директории сразу пробуем LIST.
    QByteArray listing;
    QString listError;
    bool dirListOnlyMode = false;
    bool listed = performDirectoryList(m_profile, makeUrl(remotePath, true), false, &listing, &listError);

    // Если обычный LIST не поддержан, пробуем NLST/DIRLISTONLY. Типы объектов в этом
    // режиме могут быть неизвестны, но хотя бы имена будут видны пользователю.
    if (!listed) {
        listing.clear();
        QString nlstError;
        listed = performDirectoryList(m_profile, makeUrl(remotePath, true), true, &listing, &nlstError);
        dirListOnlyMode = listed;
        if (!listed) {
            result[0].valid = false;
            result[0].exists = false;
            result[0].error = QStringLiteral("Не удалось получить список удалённого каталога %1: %2")
                .arg(remotePath, listError.isEmpty() ? nlstError : listError);
            return result;
        }
    }

    const QList<QByteArray> lines = listing.split('\n');
    for (const QByteArray& rawLine : lines) {
        DirectoryListEntry parsed = parseDirectoryListLine(QString::fromUtf8(rawLine), dirListOnlyMode);
        if (parsed.name.isEmpty() || parsed.name == QStringLiteral(".") || parsed.name == QStringLiteral("..")) {
            continue;
        }

        const QString childPath = normalizeRemotePath(remotePath + QLatin1Char('/') + parsed.name);
        StorageObjectInfo child;
        if (parsed.hasType && parsed.isDirectory) {
            child.valid = true;
            child.exists = true;
            child.isDirectory = true;
            child.absolutePath = childPath;
            child.relativePath = relativePath(childPath);
            child.name = parsed.name;
            child.size = 0;
        } else if (parsed.hasType && !parsed.isDirectory) {
            child = objectInfo(childPath, calculateHash);
            if (!child.valid || !child.exists) {
                child.valid = true;
                child.exists = true;
                child.isDirectory = false;
                child.absolutePath = childPath;
                child.relativePath = relativePath(childPath);
                child.name = parsed.name;
                child.size = parsed.size < 0 ? 0 : parsed.size;
            }
        } else {
            child = objectInfo(childPath, calculateHash);
            if (!child.valid || !child.exists) {
                // Fallback для NLST: сервер отдал имя, значит объект существует, но тип
                // мог быть недоступен. Показываем как файл, чтобы пользователь хотя бы видел listing.
                child.valid = true;
                child.exists = true;
                child.isDirectory = false;
                child.absolutePath = childPath;
                child.relativePath = relativePath(childPath);
                child.name = parsed.name;
                child.size = 0;
            }
        }

        if (child.name.isEmpty()) {
            child.name = parsed.name;
        }
        result.push_back(child);

        if (recursive && child.valid && child.exists && child.isDirectory) {
            const QVector<StorageObjectInfo> nested = enumerate(child.absolutePath, true, calculateHash);
            for (int i = 1; i < nested.size(); ++i) {
                result.push_back(nested[i]);
            }
        }
    }

    return result;
#endif
}

bool CurlRemoteStorage::exists(const QString& absolutePath) const {
    const StorageObjectInfo info = objectInfo(absolutePath, false);
    return info.valid && info.exists;
}

bool CurlRemoteStorage::makeDirectory(const QString& absolutePath, QString* error) const {
    const QString remotePath = normalizeRemotePath(absolutePath);

#ifndef SECURETRANSFER_WITH_CURL
    if (error) {
        *error = QStringLiteral("Нельзя создать удалённый каталог: проект собран без libcurl");
    }
    return false;
#else
    if (exists(remotePath)) {
        return true;
    }

    QString validationError;
    if (!m_profile.isValid(&validationError)) {
        if (error) {
            *error = validationError;
        }
        return false;
    }

    if (!ensureCurlGlobalInit()) {
        if (error) {
            *error = QStringLiteral("Не удалось выполнить глобальную инициализацию libcurl");
        }
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = QStringLiteral("Не удалось инициализировать libcurl");
        }
        return false;
    }

    const QString command = m_profile.kind == StorageKind::Ftp
        ? QStringLiteral("MKD %1").arg(remotePath)
        : QStringLiteral("mkdir %1").arg(remotePath);
    struct curl_slist* quote = nullptr;
    const QByteArray commandBytes = command.toUtf8();
    quote = curl_slist_append(quote, commandBytes.constData());

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    configureCommonOptions(curl, m_profile, m_profile.baseUrl(), errorBuffer);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_QUOTE, quote);

    const CURLcode code = curl_easy_perform(curl);
    curl_slist_free_all(quote);
    curl_easy_cleanup(curl);

    if (code == CURLE_OK || exists(remotePath)) {
        return true;
    }

    if (error) {
        *error = QStringLiteral("Не удалось создать удалённый каталог %1: %2")
            .arg(remotePath, errorFromBufferOrCode(errorBuffer, code));
    }
    return false;
#endif
}

bool CurlRemoteStorage::removeFile(const QString& absolutePath, QString* error) const {
    const QString remotePath = normalizeRemotePath(absolutePath);

#ifndef SECURETRANSFER_WITH_CURL
    if (error) {
        *error = QStringLiteral("Нельзя удалить удалённый файл: проект собран без libcurl");
    }
    return false;
#else
    if (!exists(remotePath)) {
        return true;
    }

    if (!ensureCurlGlobalInit()) {
        if (error) {
            *error = QStringLiteral("Не удалось выполнить глобальную инициализацию libcurl");
        }
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = QStringLiteral("Не удалось инициализировать libcurl");
        }
        return false;
    }

    const QString command = m_profile.kind == StorageKind::Ftp
        ? QStringLiteral("DELE %1").arg(remotePath)
        : QStringLiteral("rm %1").arg(remotePath);
    struct curl_slist* quote = nullptr;
    const QByteArray commandBytes = command.toUtf8();
    quote = curl_slist_append(quote, commandBytes.constData());

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    configureCommonOptions(curl, m_profile, m_profile.baseUrl(), errorBuffer);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_QUOTE, quote);

    const CURLcode code = curl_easy_perform(curl);
    curl_slist_free_all(quote);
    curl_easy_cleanup(curl);

    if (code == CURLE_OK || !exists(remotePath)) {
        return true;
    }

    if (error) {
        *error = QStringLiteral("Не удалось удалить удалённый файл %1: %2")
            .arg(remotePath, errorFromBufferOrCode(errorBuffer, code));
    }
    return false;
#endif
}

bool CurlRemoteStorage::writeFileFromLocal(const QString& localSourcePath,
                                           const QString& destinationPath,
                                           const QDateTime& modified,
                                           QString* error) const {
    Q_UNUSED(modified)
    const QString remotePath = normalizeRemotePath(destinationPath);

#ifndef SECURETRANSFER_WITH_CURL
    if (error) {
        *error = QStringLiteral("Нельзя загрузить файл: проект собран без libcurl");
    }
    return false;
#else
    QString validationError;
    if (!m_profile.isValid(&validationError)) {
        if (error) {
            *error = validationError;
        }
        return false;
    }

    QFile file(localSourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Не удалось открыть локальный файл для загрузки: %1").arg(localSourcePath);
        }
        return false;
    }

    if (!ensureRemoteParentDirectories(remotePath, error)) {
        return false;
    }

    if (!ensureCurlGlobalInit()) {
        if (error) {
            *error = QStringLiteral("Не удалось выполнить глобальную инициализацию libcurl");
        }
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = QStringLiteral("Не удалось инициализировать libcurl");
        }
        return false;
    }

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    configureCommonOptions(curl, m_profile, makeUrl(remotePath, false), errorBuffer);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readFromQFile);
    curl_easy_setopt(curl, CURLOPT_READDATA, &file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(file.size()));
    curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR_RETRY);

    const CURLcode code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (code == CURLE_OK) {
        return true;
    }

    if (error) {
        *error = QStringLiteral("Не удалось загрузить файл %1: %2")
            .arg(remotePath, errorFromBufferOrCode(errorBuffer, code));
    }
    return false;
#endif
}

bool CurlRemoteStorage::readFileToLocal(const QString& sourcePath,
                                        const QString& localDestinationPath,
                                        QString* error) const {
    const QString remotePath = normalizeRemotePath(sourcePath);

#ifndef SECURETRANSFER_WITH_CURL
    if (error) {
        *error = QStringLiteral("Нельзя скачать файл: проект собран без libcurl");
    }
    return false;
#else
    QString validationError;
    if (!m_profile.isValid(&validationError)) {
        if (error) {
            *error = validationError;
        }
        return false;
    }

    const QFileInfo destinationInfo(localDestinationPath);
    QDir().mkpath(destinationInfo.absolutePath());

    QFile file(localDestinationPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Не удалось открыть локальный файл для скачивания: %1").arg(localDestinationPath);
        }
        return false;
    }

    if (!ensureCurlGlobalInit()) {
        if (error) {
            *error = QStringLiteral("Не удалось выполнить глобальную инициализацию libcurl");
        }
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        if (error) {
            *error = QStringLiteral("Не удалось инициализировать libcurl");
        }
        return false;
    }

    char errorBuffer[CURL_ERROR_SIZE] = {0};
    configureCommonOptions(curl, m_profile, makeUrl(remotePath, false), errorBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToQFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    const CURLcode code = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    file.close();

    if (code == CURLE_OK) {
        return true;
    }

    QFile::remove(localDestinationPath);
    if (error) {
        *error = QStringLiteral("Не удалось скачать файл %1: %2")
            .arg(remotePath, errorFromBufferOrCode(errorBuffer, code));
    }
    return false;
#endif
}

QString CurlRemoteStorage::normalizeRemotePath(const QString& path) const {
    QString normalized = path.trimmed();
    if (normalized.isEmpty()) {
        normalized = QStringLiteral("/");
    }
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (!normalized.startsWith(QLatin1Char('/'))) {
        normalized.prepend(QLatin1Char('/'));
    }
    normalized = QDir::cleanPath(normalized);
    if (normalized == QStringLiteral(".")) {
        normalized = QStringLiteral("/");
    }
    return normalized;
}

QString CurlRemoteStorage::makeUrl(const QString& remotePath, bool directoryUrl) const {
    const QString path = normalizeRemotePath(remotePath);
    QUrl url;
    url.setScheme(m_profile.scheme());
    url.setHost(m_profile.host.trimmed());
    if (m_profile.port > 0) {
        url.setPort(m_profile.port);
    }
    QString urlPath = path;
    if (directoryUrl && !urlPath.endsWith(QLatin1Char('/'))) {
        urlPath.append(QLatin1Char('/'));
    }
    url.setPath(urlPath);
    return url.toString(QUrl::FullyEncoded);
}

QString CurlRemoteStorage::fileSha256(const QString& localPath) const {
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }

    return QString::fromLatin1(hash.result().toHex());
}

QVector<QString> CurlRemoteStorage::parentDirectories(const QString& remotePath) const {
    QVector<QString> parents;
    const QString normalized = normalizeRemotePath(remotePath);
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    QString current;
    for (int i = 0; i + 1 < parts.size(); ++i) {
        current += QLatin1Char('/') + parts[i];
        parents.push_back(current);
    }
    return parents;
}

bool CurlRemoteStorage::ensureRemoteParentDirectories(const QString& remotePath, QString* error) const {
    const QVector<QString> parents = parentDirectories(remotePath);
    for (const QString& directory : parents) {
        if (!makeDirectory(directory, error)) {
            if (!exists(directory)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace stl
