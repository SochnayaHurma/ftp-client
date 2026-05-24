#pragma once

#include "RemoteConnectionProfile.h"

#include <QString>
#include <QVector>

namespace stl {

struct NamedConnectionProfile {
    QString name;
    RemoteConnectionProfile profile;
    bool rememberPassword = false;
};

class ConnectionProfileStore {
public:
    explicit ConnectionProfileStore(const QString& settingsPath);

    QVector<NamedConnectionProfile> loadProfiles() const;
    bool saveProfile(const QString& name,
                     const RemoteConnectionProfile& profile,
                     bool rememberPassword,
                     QString* error = nullptr) const;
    bool deleteProfile(const QString& name, QString* error = nullptr) const;
    QString storagePath() const;

private:
    void writeProfiles(const QVector<NamedConnectionProfile>& profiles) const;
    QString normalizedName(const QString& name) const;

    QString m_settingsPath;
};

} // namespace stl
