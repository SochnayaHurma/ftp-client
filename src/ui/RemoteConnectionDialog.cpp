#include "RemoteConnectionDialog.h"
#include "ui_RemoteConnectionDialog.h"

#include "../core/storage/CurlConnectionTester.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

RemoteConnectionDialog::RemoteConnectionDialog(QWidget* parent)
    : QDialog(parent) {
    ui = std::make_unique<Ui::RemoteConnectionDialog>();
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("FTP/SFTP профиль"));
    resize(520, 360);

    m_savedProfiles = ui->savedProfilesCombo;
    m_profileName = ui->profileNameEdit;
    m_protocol = ui->protocolCombo;
    m_host = ui->hostEdit;
    m_port = ui->portSpin;
    m_username = ui->usernameEdit;
    m_password = ui->passwordEdit;
    m_remoteRoot = ui->remoteRootEdit;
    m_passiveMode = ui->passiveModeCheckBox;
    m_saveProfile = ui->saveProfileCheckBox;
    m_rememberPassword = ui->rememberPasswordCheckBox;
    m_status = ui->statusLabel;

    m_savedProfiles->addItem(QStringLiteral("Новый профиль"), -1);

    m_protocol->addItem(QStringLiteral("SFTP"), static_cast<int>(stl::StorageKind::Sftp));
    m_protocol->addItem(QStringLiteral("FTP"), static_cast<int>(stl::StorageKind::Ftp));

    m_port->setRange(1, 65535);
    m_port->setValue(22);
    m_password->setEchoMode(QLineEdit::Password);
    m_remoteRoot->setText(QStringLiteral("/"));
    m_passiveMode->setChecked(true);
    m_saveProfile->setChecked(true);
    m_rememberPassword->setToolTip(QStringLiteral("Для учебного проекта пароль можно сохранить локально. В промышленном ПО лучше использовать защищённое хранилище ОС."));
    m_status->setText(QStringLiteral("Проверка соединения необязательна для сохранения профиля."));
    m_status->setWordWrap(true);

    connect(m_savedProfiles, qOverload<int>(&QComboBox::currentIndexChanged), this, &RemoteConnectionDialog::onSavedProfileSelected);
    connect(m_protocol, qOverload<int>(&QComboBox::currentIndexChanged), this, &RemoteConnectionDialog::updateDefaultPort);
    connect(ui->testConnectionButton, &QPushButton::clicked, this, &RemoteConnectionDialog::testConnection);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RemoteConnectionDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &RemoteConnectionDialog::reject);
}

RemoteConnectionDialog::~RemoteConnectionDialog() = default;

stl::RemoteConnectionProfile RemoteConnectionDialog::profile() const {
    stl::RemoteConnectionProfile result;
    result.kind = static_cast<stl::StorageKind>(m_protocol->currentData().toInt());
    result.host = m_host->text();
    result.port = m_port->value();
    result.username = m_username->text();
    result.password = m_password->text();
    result.remoteRoot = m_remoteRoot->text();
    result.passiveMode = m_passiveMode->isChecked();
    return result;
}

void RemoteConnectionDialog::setProfile(const stl::RemoteConnectionProfile& profile) {
    const int protocolIndex = m_protocol->findData(static_cast<int>(profile.kind));
    if (protocolIndex >= 0) {
        m_protocol->setCurrentIndex(protocolIndex);
    }
    m_host->setText(profile.host);
    m_port->setValue(profile.port);
    m_username->setText(profile.username);
    m_password->setText(profile.password);
    m_remoteRoot->setText(profile.remoteRoot);
    m_passiveMode->setChecked(profile.passiveMode);
}

void RemoteConnectionDialog::setSavedProfiles(const QVector<stl::NamedConnectionProfile>& profiles) {
    m_profileList = profiles;
    const QSignalBlocker blocker(m_savedProfiles);
    m_savedProfiles->clear();
    m_savedProfiles->addItem(QStringLiteral("Новый профиль"), -1);

    for (int i = 0; i < m_profileList.size(); ++i) {
        const stl::NamedConnectionProfile& item = m_profileList[i];
        m_savedProfiles->addItem(QStringLiteral("%1 — %2").arg(item.name, item.profile.safeDescription()), i);
    }
}

QString RemoteConnectionDialog::profileName() const {
    const QString explicitName = m_profileName->text().trimmed();
    if (!explicitName.isEmpty()) {
        return explicitName;
    }

    const stl::RemoteConnectionProfile current = profile();
    return current.host.trimmed().isEmpty()
        ? QStringLiteral("Безымянный профиль")
        : current.safeDescription();
}

bool RemoteConnectionDialog::shouldSaveProfile() const {
    return m_saveProfile->isChecked();
}

bool RemoteConnectionDialog::shouldRememberPassword() const {
    return m_rememberPassword->isChecked();
}

void RemoteConnectionDialog::updateDefaultPort(int protocolIndex) {
    const auto kind = static_cast<stl::StorageKind>(m_protocol->itemData(protocolIndex).toInt());
    if (kind == stl::StorageKind::Sftp && m_port->value() == 21) {
        m_port->setValue(22);
    } else if (kind == stl::StorageKind::Ftp && m_port->value() == 22) {
        m_port->setValue(21);
    }
}

void RemoteConnectionDialog::testConnection() {
    QString validationError;
    const stl::RemoteConnectionProfile current = profile();
    if (!current.isValid(&validationError)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка профиля"), validationError);
        return;
    }

    const stl::ConnectionTestResult result = stl::CurlConnectionTester::test(current);
    m_status->setText(result.message);

    if (result.ok) {
        QMessageBox::information(this, QStringLiteral("Проверка подключения"), result.message);
    } else {
        QMessageBox::warning(this, QStringLiteral("Проверка подключения"), result.message);
    }
}

void RemoteConnectionDialog::onSavedProfileSelected(int index) {
    const int profileIndex = m_savedProfiles->itemData(index).toInt();
    if (profileIndex < 0 || profileIndex >= m_profileList.size()) {
        return;
    }

    const stl::NamedConnectionProfile& selected = m_profileList[profileIndex];
    m_profileName->setText(selected.name);
    setProfile(selected.profile);
    m_saveProfile->setChecked(true);
    m_rememberPassword->setChecked(selected.rememberPassword);
}
