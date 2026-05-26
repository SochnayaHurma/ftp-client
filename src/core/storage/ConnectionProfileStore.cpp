#include "ConnectionProfileStore.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace stl {

ConnectionProfileStore::ConnectionProfileStore(const QString& settingsPath)
    : m_settingsPath(settingsPath) {
}

QString ConnectionProfileStore::storagePath() const {
    return m_settingsPath;
}

QVector<NamedConnectionProfile> ConnectionProfileStore::loadProfiles() const {
    QVector<NamedConnectionProfile> result;
    QSettings settings(m_settingsPath, QSettings::IniFormat);

    const int count = settings.beginReadArray(QStringLiteral("profiles"));
    result.reserve(count);

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        NamedConnectionProfile item;
        item.name = settings.value(QStringLiteral("name")).toString();
        item.profile.kind = static_cast<StorageKind>(settings.value(QStringLiteral("kind"), static_cast<int>(StorageKind::Sftp)).toInt());
        item.profile.host = settings.value(QStringLiteral("host")).toString();
        item.profile.port = settings.value(QStringLiteral("port"), item.profile.kind == StorageKind::Ftp ? 21 : 22).toInt();
        item.profile.username = settings.value(QStringLiteral("username")).toString();
        item.profile.password = settings.value(QStringLiteral("password")).toString();
        item.profile.remoteRoot = settings.value(QStringLiteral("remoteRoot"), QStringLiteral("/")).toString();
        item.profile.passiveMode = settings.value(QStringLiteral("passiveMode"), true).toBool();
        item.profile.timeoutSeconds = settings.value(QStringLiteral("timeoutSeconds"), 10).toInt();
        item.rememberPassword = settings.value(QStringLiteral("rememberPassword"), false).toBool();

        if (!item.name.trimmed().isEmpty()) {
            result.push_back(item);
        }
    }

    settings.endArray();
    return result;
}

bool ConnectionProfileStore::saveProfile(const QString& name,
                                         const RemoteConnectionProfile& profile,
                                         bool rememberPassword,
                                         QString* error) const {
    const QString cleanName = normalizedName(name);
    if (cleanName.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Не указано имя профиля");
        }
        return false;
    }

    QString validationError;
    if (!profile.isValid(&validationError)) {
        if (error) {
            *error = validationError;
        }
        return false;
    }

    QVector<NamedConnectionProfile> profiles = loadProfiles();
    NamedConnectionProfile saved;
    saved.name = cleanName;
    saved.profile = profile;
    saved.rememberPassword = rememberPassword;
    if (!rememberPassword) {
        saved.profile.password.clear();
    }

    bool replaced = false;
    for (NamedConnectionProfile& existing : profiles) {
        if (existing.name.compare(cleanName, Qt::CaseInsensitive) == 0) {
            existing = saved;
            replaced = true;
            break;
        }
    }

    if (!replaced) {
        profiles.push_back(saved);
    }

    writeProfiles(profiles);
    return true;
}

bool ConnectionProfileStore::deleteProfile(const QString& name, QString* error) const {
    const QString cleanName = normalizedName(name);
    if (cleanName.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Не указано имя профиля");
        }
        return false;
    }

    QVector<NamedConnectionProfile> profiles = loadProfiles();
    const int before = profiles.size();
    profiles.erase(std::remove_if(profiles.begin(), profiles.end(), [&](const NamedConnectionProfile& item) {
        return item.name.compare(cleanName, Qt::CaseInsensitive) == 0;
    }), profiles.end());

    if (profiles.size() == before) {
        if (error) {
            *error = QStringLiteral("Профиль не найден");
        }
        return false;
    }

    writeProfiles(profiles);
    return true;
}

void ConnectionProfileStore::writeProfiles(const QVector<NamedConnectionProfile>& profiles) const {
    QDir().mkpath(QFileInfo(m_settingsPath).absolutePath());
    QSettings settings(m_settingsPath, QSettings::IniFormat);
    settings.clear();
    settings.beginWriteArray(QStringLiteral("profiles"));

    for (int i = 0; i < profiles.size(); ++i) {
        const NamedConnectionProfile& item = profiles[i];
        settings.setArrayIndex(i);
        settings.setValue(QStringLiteral("name"), item.name);
        settings.setValue(QStringLiteral("kind"), static_cast<int>(item.profile.kind));
        settings.setValue(QStringLiteral("host"), item.profile.host);
        settings.setValue(QStringLiteral("port"), item.profile.port);
        settings.setValue(QStringLiteral("username"), item.profile.username);
        settings.setValue(QStringLiteral("password"), item.rememberPassword ? item.profile.password : QString());
        settings.setValue(QStringLiteral("remoteRoot"), item.profile.remoteRoot);
        settings.setValue(QStringLiteral("passiveMode"), item.profile.passiveMode);
        settings.setValue(QStringLiteral("timeoutSeconds"), item.profile.timeoutSeconds);
        settings.setValue(QStringLiteral("rememberPassword"), item.rememberPassword);
    }

    settings.endArray();
    settings.sync();
}

QString ConnectionProfileStore::normalizedName(const QString& name) const {
    return name.trimmed();
}

}
