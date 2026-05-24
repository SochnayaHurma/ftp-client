#pragma once

#include "../core/storage/ConnectionProfileStore.h"
#include "../core/storage/RemoteConnectionProfile.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVector>
#include <memory>

namespace Ui { class RemoteConnectionDialog; }

class RemoteConnectionDialog final : public QDialog {
    Q_OBJECT

public:
    explicit RemoteConnectionDialog(QWidget* parent = nullptr);
    ~RemoteConnectionDialog() override;

    stl::RemoteConnectionProfile profile() const;
    void setProfile(const stl::RemoteConnectionProfile& profile);

    void setSavedProfiles(const QVector<stl::NamedConnectionProfile>& profiles);
    QString profileName() const;
    bool shouldSaveProfile() const;
    bool shouldRememberPassword() const;

private slots:
    void updateDefaultPort(int protocolIndex);
    void testConnection();
    void onSavedProfileSelected(int index);

private:
    std::unique_ptr<Ui::RemoteConnectionDialog> ui;

    QComboBox* m_savedProfiles = nullptr;
    QLineEdit* m_profileName = nullptr;
    QComboBox* m_protocol = nullptr;
    QLineEdit* m_host = nullptr;
    QSpinBox* m_port = nullptr;
    QLineEdit* m_username = nullptr;
    QLineEdit* m_password = nullptr;
    QLineEdit* m_remoteRoot = nullptr;
    QCheckBox* m_passiveMode = nullptr;
    QCheckBox* m_saveProfile = nullptr;
    QCheckBox* m_rememberPassword = nullptr;
    QLabel* m_status = nullptr;

    QVector<stl::NamedConnectionProfile> m_profileList;
};
