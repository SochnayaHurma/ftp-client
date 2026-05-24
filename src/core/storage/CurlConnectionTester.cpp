#include "CurlConnectionTester.h"

#ifdef SECURETRANSFER_WITH_CURL
#include <curl/curl.h>
#endif

namespace stl {

#ifdef SECURETRANSFER_WITH_CURL
namespace {
size_t discardCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    Q_UNUSED(ptr)
    Q_UNUSED(userdata)
    return size * nmemb;
}
}
#endif

ConnectionTestResult CurlConnectionTester::test(const RemoteConnectionProfile& profile) {
    QString validationError;
    if (!profile.isValid(&validationError)) {
        return {false, validationError};
    }

#ifndef SECURETRANSFER_WITH_CURL
    Q_UNUSED(profile)
    return {
        false,
        QStringLiteral("Модуль libcurl не включён при сборке. Соберите проект с -DSECURETRANSFER_WITH_CURL=ON для проверки FTP/SFTP подключения.")
    };
#else
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();
    if (!curl) {
        return {false, QStringLiteral("Не удалось инициализировать libcurl")};
    }

    const QByteArray url = profile.baseUrl().toUtf8();
    const QByteArray username = profile.username.toUtf8();
    const QByteArray password = profile.password.toUtf8();

    curl_easy_setopt(curl, CURLOPT_URL, url.constData());
    curl_easy_setopt(curl, CURLOPT_USERNAME, username.constData());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password.constData());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(profile.timeoutSeconds));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(profile.timeoutSeconds));
    curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardCallback);
    curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, profile.passiveMode ? 1L : 0L);

    const CURLcode code = curl_easy_perform(curl);
    const QString message = code == CURLE_OK
        ? QStringLiteral("Подключение успешно проверено: %1").arg(profile.safeDescription())
        : QStringLiteral("Ошибка подключения: %1").arg(QString::fromUtf8(curl_easy_strerror(code)));

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return {code == CURLE_OK, message};
#endif
}

} // namespace stl
