#include "MainWindow.h"
#include "HistoryDialog.h"
#include "RemoteConnectionDialog.h"
#include "ui_MainWindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QEventLoop>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QProgressBar>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtGlobal>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_profileStore(QApplication::applicationDirPath() + QStringLiteral("/connection_profiles.ini")),
      m_journal(QApplication::applicationDirPath() + QStringLiteral("/transfer_journal.db")) {
    buildUi();

    const QString localRoot = QDir::homePath();
    const QString remoteRoot = defaultRemoteRoot();
    QDir().mkpath(remoteRoot);

    setLocalRoot(localRoot);
    setRemoteEmulatorRoot(remoteRoot);

    m_remoteProfile.kind = stl::StorageKind::Sftp;
    m_remoteProfile.port = 22;
    m_remoteProfile.remoteRoot = QStringLiteral("/");

    if (!m_journal.open()) {
        appendLog(QStringLiteral("SQLite-журнал не открыт: %1").arg(m_journal.lastError()));
    } else {
        appendLog(QStringLiteral("Журнал операций готов: %1").arg(m_journal.databasePath()));
    }

    appendLog(QStringLiteral("Хранилище профилей: %1").arg(m_profileStore.storagePath()));
    appendLog(QStringLiteral("Stage 4: добавлены фильтры файлов, сохранённые профили подключений и окно истории операций SQLite."));
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    ui = std::make_unique<Ui::MainWindow>();
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("SecureTransfer"));
    resize(1320, 880);

    m_localPathEdit = ui->localPathEdit;
    m_remotePathEdit = ui->remotePathEdit;
    m_remoteBackendLabel = ui->remoteBackendLabel;
    m_fileFilterEdit = ui->fileFilterEdit;
    m_filterModeCombo = ui->filterModeCombo;
    m_localView = ui->localView;
    m_remoteView = ui->remoteView;
    m_planTable = ui->planTable;
    m_queueTable = ui->queueTable;
    m_log = ui->logTextEdit;
    m_backupCheckBox = ui->backupCheckBox;
    m_totalProgressBar = ui->totalProgressBar;
    m_queueSummaryLabel = ui->queueSummaryLabel;
    m_executeButton = ui->executeButton;
    m_cancelButton = ui->cancelButton;

    auto* chooseLocalButton = ui->chooseLocalButton;
    auto* chooseRemoteButton = ui->chooseRemoteButton;
    auto* remoteUpButton = ui->remoteUpButton;
    auto* connectRemoteButton = ui->connectRemoteButton;
    auto* emulatorButton = ui->emulatorButton;
    auto* applyFilterButton = ui->applyFilterButton;
    auto* clearFilterButton = ui->clearFilterButton;
    auto* historyButton = ui->historyButton;
    auto* analyzeUploadButton = ui->analyzeUploadButton;
    auto* analyzeDownloadButton = ui->analyzeDownloadButton;
    auto* refreshButton = ui->refreshButton;

    m_filterModeCombo->addItem(QStringLiteral("Все объекты"), static_cast<int>(stl::FileFilterMode::All));
    m_filterModeCombo->addItem(QStringLiteral("Только файлы"), static_cast<int>(stl::FileFilterMode::FilesOnly));
    m_filterModeCombo->addItem(QStringLiteral("Только папки"), static_cast<int>(stl::FileFilterMode::DirectoriesOnly));

    m_localModel = new QFileSystemModel(this);
    m_localProxyModel = new FileSystemFilterProxyModel(this);
    m_remoteModel = new RemoteFileModel(this);

    m_planTable->setColumnCount(9);
    m_planTable->setHorizontalHeaderLabels({
        QStringLiteral("Статус"),
        QStringLiteral("Риск"),
        QStringLiteral("Объект"),
        QStringLiteral("Размер источника"),
        QStringLiteral("Размер назначения"),
        QStringLiteral("SHA-256 источника"),
        QStringLiteral("SHA-256 назначения"),
        QStringLiteral("Назначение"),
        QStringLiteral("Предупреждение")
    });
    m_planTable->horizontalHeader()->setStretchLastSection(true);
    m_planTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_planTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_planTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_queueTable->setColumnCount(7);
    m_queueTable->setHorizontalHeaderLabels({
        QStringLiteral("№"),
        QStringLiteral("Действие"),
        QStringLiteral("Объект"),
        QStringLiteral("Размер"),
        QStringLiteral("Статус"),
        QStringLiteral("Прогресс"),
        QStringLiteral("Сообщение")
    });
    m_queueTable->horizontalHeader()->setStretchLastSection(true);
    m_queueTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_totalProgressBar->setRange(0, 100);
    m_totalProgressBar->setValue(0);
    m_queueSummaryLabel->setText(QStringLiteral("Очередь не сформирована"));

    m_log->setReadOnly(true);
    m_log->setMinimumHeight(120);

    connect(chooseLocalButton, &QPushButton::clicked, this, &MainWindow::chooseLocalRoot);
    connect(chooseRemoteButton, &QPushButton::clicked, this, &MainWindow::chooseRemoteRoot);
    connect(remoteUpButton, &QPushButton::clicked, this, &MainWindow::remoteGoUp);
    connect(connectRemoteButton, &QPushButton::clicked, this, &MainWindow::configureRemoteConnection);
    connect(emulatorButton, &QPushButton::clicked, this, &MainWindow::switchToEmulator);
    connect(m_remoteView, &QTreeView::doubleClicked, this, &MainWindow::onRemoteDoubleClicked);
    connect(analyzeUploadButton, &QPushButton::clicked, this, &MainWindow::analyzeUpload);
    connect(analyzeDownloadButton, &QPushButton::clicked, this, &MainWindow::analyzeDownload);
    connect(m_executeButton, &QPushButton::clicked, this, &MainWindow::executeCurrentPlan);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::cancelTransferQueue);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshModels);
    connect(applyFilterButton, &QPushButton::clicked, this, &MainWindow::applyFileFilter);
    connect(clearFilterButton, &QPushButton::clicked, this, &MainWindow::clearFileFilter);
    connect(historyButton, &QPushButton::clicked, this, &MainWindow::showHistory);
    connect(m_fileFilterEdit, &QLineEdit::returnPressed, this, &MainWindow::applyFileFilter);
}

void MainWindow::configureLocalModel(QFileSystemModel* model, QTreeView* view, const QString& rootPath) {
    model->setRootPath(rootPath);

    m_localProxyModel->setSourceModel(model);

    view->setModel(m_localProxyModel);

    view->setRootIndex(m_localProxyModel->mapFromSource(model->index(rootPath)));
    view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view->setSortingEnabled(true);
    view->sortByColumn(0, Qt::AscendingOrder);
    view->setColumnWidth(0, 320);

}

void MainWindow::setLocalRoot(const QString& path) {
    const QString normalized = QFileInfo(path).absoluteFilePath();
    m_localPathEdit->setText(normalized);
    m_localBackend.setRoot(normalized);
    configureLocalModel(m_localModel, m_localView, normalized);
    applyFilterToModels();
}

void MainWindow::setRemoteEmulatorRoot(const QString& path) {
    const QString normalized = QFileInfo(path).absoluteFilePath();
    m_useCurlRemoteBackend = false;
    m_remotePathEdit->setText(normalized);
    m_remoteEmulatorBackend.setRoot(normalized);
    refreshRemotePanel();
}

void MainWindow::updateBackendRoots() {
    m_localBackend.setRoot(m_localPathEdit->text());
    if (m_useCurlRemoteBackend && m_curlRemoteBackend) {
        m_curlRemoteBackend->setRoot(m_remotePathEdit->text());
    } else {
        m_remoteEmulatorBackend.setRoot(m_remotePathEdit->text());
    }
}

QString MainWindow::defaultRemoteRoot() const {
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documents.isEmpty() ? QDir::homePath() : documents).absoluteFilePath(QStringLiteral("SecureTransferServerEmulator"));
}

const stl::IStorageBackend& MainWindow::activeRemoteBackend() const {
    if (m_useCurlRemoteBackend && m_curlRemoteBackend) {
        return *m_curlRemoteBackend;
    }
    return m_remoteEmulatorBackend;
}

stl::IStorageBackend& MainWindow::activeRemoteBackend() {
    if (m_useCurlRemoteBackend && m_curlRemoteBackend) {
        return *m_curlRemoteBackend;
    }
    return m_remoteEmulatorBackend;
}

QString MainWindow::activeRemoteDescription() const {
    if (m_useCurlRemoteBackend && m_curlRemoteBackend) {
        return QStringLiteral("активный FTP/SFTP backend: %1").arg(m_remoteProfile.safeDescription());
    }
    return QStringLiteral("активный backend: локальный сервер-эмулятор");
}

void MainWindow::refreshRemotePanel() {
    updateBackendRoots();
    m_remoteModel->setBackend(&activeRemoteBackend());
    m_remoteModel->setCurrentPath(m_remotePathEdit->text());
    m_remoteModel->setFilterText(m_fileFilterEdit ? m_fileFilterEdit->text() : QString());
    m_remoteModel->setFilterMode(filterModeFromUi());
    m_remoteView->setModel(m_remoteModel);
    m_remoteView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_remoteView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_remoteView->setSortingEnabled(true);

    QString error;
    if (!m_remoteModel->reload(&error)) {
        appendLog(QStringLiteral("Ошибка обновления удалённой панели: %1").arg(error));
    }

    m_remoteView->sortByColumn(0, Qt::AscendingOrder);
    m_remoteView->setColumnWidth(0, 320);
    m_remoteView->setColumnWidth(1, 90);
    m_remoteView->setColumnWidth(2, 110);
    m_remoteBackendLabel->setText(activeRemoteDescription());
}

void MainWindow::chooseLocalRoot() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Выберите локальный каталог"), m_localPathEdit->text());
    if (!path.isEmpty()) {
        setLocalRoot(path);
    }
}

void MainWindow::chooseRemoteRoot() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Выберите серверный каталог-эмулятор"), m_remotePathEdit->text());
    if (!path.isEmpty()) {
        setRemoteEmulatorRoot(path);
        appendLog(QStringLiteral("Правая панель переключена на локальный сервер-эмулятор: %1").arg(path));
    }
}

void MainWindow::switchToEmulator() {
    const QString root = m_remoteEmulatorBackend.root().isEmpty() ? defaultRemoteRoot() : m_remoteEmulatorBackend.root();
    setRemoteEmulatorRoot(root);
    appendLog(QStringLiteral("Включён сервер-эмулятор: %1").arg(root));
}

void MainWindow::configureRemoteConnection() {
    RemoteConnectionDialog dialog(this);
    dialog.setSavedProfiles(m_profileStore.loadProfiles());
    dialog.setProfile(m_remoteProfile);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_remoteProfile = dialog.profile();
    QString validationError;
    if (!m_remoteProfile.isValid(&validationError)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка профиля"), validationError);
        return;
    }

    if (dialog.shouldSaveProfile()) {
        QString profileError;
        if (m_profileStore.saveProfile(dialog.profileName(), m_remoteProfile, dialog.shouldRememberPassword(), &profileError)) {
            appendLog(QStringLiteral("Профиль подключения сохранён: %1").arg(dialog.profileName()));
        } else {
            appendLog(QStringLiteral("Профиль подключения не сохранён: %1").arg(profileError));
        }
    }

    if (!stl::CurlRemoteStorage::isBuiltWithCurl()) {
        QMessageBox::information(this,
            QStringLiteral("libcurl не подключён"),
            QStringLiteral("Профиль сохранён, но проект собран без libcurl. Для настоящего FTP/SFTP backend пересоберите проект с параметром -DSECURETRANSFER_WITH_CURL=ON."));
        m_remoteBackendLabel->setText(QStringLiteral("FTP/SFTP профиль подготовлен: %1. Активен сервер-эмулятор, так как libcurl не подключён.")
            .arg(m_remoteProfile.safeDescription()));
        appendLog(QStringLiteral("Профиль удалённого подключения подготовлен, но libcurl не подключён: %1").arg(m_remoteProfile.safeDescription()));
        return;
    }

    m_curlRemoteBackend = std::make_unique<stl::CurlRemoteStorage>(m_remoteProfile);
    QString connectionError;
    const bool connectionOk = m_curlRemoteBackend->testConnection(&connectionError);
    if (!connectionOk) {
        const auto answer = QMessageBox::question(this,
            QStringLiteral("Подключение не проверено"),
            QStringLiteral("Не удалось проверить подключение: %1\n\nИспользовать профиль всё равно?").arg(connectionError));
        if (answer != QMessageBox::Yes) {
            m_curlRemoteBackend.reset();
            return;
        }
    }

    m_useCurlRemoteBackend = true;
    m_remotePathEdit->setText(m_remoteProfile.remoteRoot.isEmpty() ? QStringLiteral("/") : m_remoteProfile.remoteRoot);
    refreshRemotePanel();
    appendLog(QStringLiteral("Включён CurlRemoteStorage: %1").arg(m_remoteProfile.safeDescription()));
}

void MainWindow::remoteGoUp() {
    QString current = m_remotePathEdit->text();
    if (current.isEmpty() || current == QStringLiteral("/")) {
        return;
    }

    QString parent;
    if (m_useCurlRemoteBackend) {
        parent = QFileInfo(current).absolutePath();
        if (parent.isEmpty() || parent == QStringLiteral(".")) {
            parent = QStringLiteral("/");
        }
    } else {
        parent = QFileInfo(current).absolutePath();
    }

    m_remotePathEdit->setText(parent);
    refreshRemotePanel();
}

void MainWindow::onRemoteDoubleClicked(const QModelIndex& index) {
    if (!m_remoteModel->isDirectory(index)) {
        return;
    }

    const QString path = m_remoteModel->absolutePath(index);
    if (path.isEmpty()) {
        return;
    }

    m_remotePathEdit->setText(path);
    refreshRemotePanel();
}

QStringList MainWindow::selectedLocalPaths() const {
    QStringList paths;
    const QModelIndexList indexes = m_localView->selectionModel()->selectedRows(0);

    for (const QModelIndex& proxyIndex : indexes) {
        const QModelIndex sourceIndex = m_localProxyModel->mapToSource(proxyIndex);
        if (sourceIndex.isValid()) {
            paths.push_back(m_localModel->filePath(sourceIndex));
        }
    }

    return paths;
}

QStringList MainWindow::selectedRemotePaths() const {
    QStringList paths;
    const QModelIndexList indexes = m_remoteView->selectionModel()->selectedRows(0);

    for (const QModelIndex& index : indexes) {
        const QString path = m_remoteModel->absolutePath(index);
        if (!path.isEmpty()) {
            paths.push_back(path);
        }
    }

    return paths;
}

void MainWindow::analyzeUpload() {
    analyze(stl::TransferDirection::Upload);
}

void MainWindow::analyzeDownload() {
    analyze(stl::TransferDirection::Download);
}

void MainWindow::analyze(stl::TransferDirection direction) {
    m_currentDirection = direction;
    updateBackendRoots();

    const bool upload = direction == stl::TransferDirection::Upload;
    const QStringList selected = upload ? selectedLocalPaths() : selectedRemotePaths();

    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Нет выбора"), QStringLiteral("Выберите один или несколько файлов/папок для анализа."));
        return;
    }

    const stl::IStorageBackend& sourceBackend = upload ? static_cast<const stl::IStorageBackend&>(m_localBackend)
                                                       : activeRemoteBackend();
    const stl::IStorageBackend& destinationBackend = upload ? activeRemoteBackend()
                                                            : static_cast<const stl::IStorageBackend&>(m_localBackend);

    m_currentPlan = m_analyzer.analyze(selected, sourceBackend, destinationBackend);
    displayPlan(m_currentPlan);
    displayQueue(m_currentPlan);
    appendLog(QStringLiteral("Сформирован план через backend-слой: %1, объектов: %2")
        .arg(stl::directionToText(direction))
        .arg(m_currentPlan.size()));
}

void MainWindow::displayPlan(const QVector<stl::PreflightItem>& plan) {
    m_planTable->setRowCount(plan.size());

    auto shortHash = [](const QString& hash) {
        return hash.isEmpty() ? QStringLiteral("-") : hash.left(16) + QStringLiteral("...");
    };

    for (int row = 0; row < plan.size(); ++row) {
        const stl::PreflightItem& item = plan[row];
        m_planTable->setItem(row, 0, new QTableWidgetItem(stl::statusToText(item.status)));
        m_planTable->setItem(row, 1, new QTableWidgetItem(stl::riskToText(item.risk)));
        m_planTable->setItem(row, 2, new QTableWidgetItem(item.relativePath));
        m_planTable->setItem(row, 3, new QTableWidgetItem(item.isDirectory ? QStringLiteral("-") : QString::number(item.sourceSize)));
        m_planTable->setItem(row, 4, new QTableWidgetItem(item.destinationSize < 0 ? QStringLiteral("-") : QString::number(item.destinationSize)));
        m_planTable->setItem(row, 5, new QTableWidgetItem(shortHash(item.sourceSha256)));
        m_planTable->setItem(row, 6, new QTableWidgetItem(shortHash(item.destinationSha256)));
        m_planTable->setItem(row, 7, new QTableWidgetItem(item.destinationPath));
        m_planTable->setItem(row, 8, new QTableWidgetItem(item.warning));
        colorizePlanRow(row, item);
    }
}

void MainWindow::colorizePlanRow(int row, const stl::PreflightItem& item) {
    QColor background;
    if (item.status == stl::PreflightStatus::Error || item.status == stl::PreflightStatus::Conflict) {
        background = QColor(255, 225, 225);
    } else if (item.risk == stl::RiskLevel::High) {
        background = QColor(255, 225, 225);
    } else if (item.risk == stl::RiskLevel::Medium || item.status == stl::PreflightStatus::Overwrite) {
        background = QColor(255, 245, 210);
    } else if (item.status == stl::PreflightStatus::NewFile || item.status == stl::PreflightStatus::NewDirectory) {
        background = QColor(225, 245, 225);
    } else if (item.status == stl::PreflightStatus::Same) {
        background = QColor(235, 235, 235);
    }

    if (!background.isValid()) {
        return;
    }

    const QBrush brush(background);
    for (int column = 0; column < m_planTable->columnCount(); ++column) {
        if (auto* cell = m_planTable->item(row, column)) {
            cell->setBackground(brush);
        }
    }
}

void MainWindow::executeCurrentPlan() {
    if (m_currentPlan.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("План пуст"), QStringLiteral("Сначала выполните предварительный анализ."));
        return;
    }

    int overwriteCount = 0;
    int errorCount = 0;
    for (const stl::PreflightItem& item : m_currentPlan) {
        if (item.status == stl::PreflightStatus::Overwrite) {
            overwriteCount++;
        }
        if (item.status == stl::PreflightStatus::Error || item.status == stl::PreflightStatus::Conflict) {
            errorCount++;
        }
    }

    if (errorCount > 0) {
        const auto answer = QMessageBox::question(this,
            QStringLiteral("В плане есть ошибки"),
            QStringLiteral("В плане есть ошибки: %1. Продолжить выполнение остальных операций?").arg(errorCount));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    if (overwriteCount > 0) {
        const auto answer = QMessageBox::question(this,
            QStringLiteral("Подтверждение перезаписи"),
            m_backupCheckBox->isChecked()
                ? QStringLiteral("Будет перезаписано файлов: %1. Перед перезаписью будут созданы резервные копии. Продолжить?").arg(overwriteCount)
                : QStringLiteral("Будет перезаписано файлов: %1. Резервные копии отключены. Продолжить?").arg(overwriteCount));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    updateBackendRoots();
    const bool upload = m_currentDirection == stl::TransferDirection::Upload;
    const stl::IStorageBackend& sourceBackend = upload ? static_cast<const stl::IStorageBackend&>(m_localBackend)
                                                       : activeRemoteBackend();
    const stl::IStorageBackend& destinationBackend = upload ? activeRemoteBackend()
                                                            : static_cast<const stl::IStorageBackend&>(m_localBackend);

    if (m_transferQueue.isEmpty()) {
        displayQueue(m_currentPlan);
    }

    m_cancelRequested = false;
    setTransferControlsRunning(true);
    appendLog(QStringLiteral("Очередь передачи запущена: %1, элементов: %2")
        .arg(stl::directionToText(m_currentDirection))
        .arg(m_currentPlan.size()));

    const stl::TransferResult result = m_transferManager.execute(
        m_currentPlan,
        sourceBackend,
        destinationBackend,
        m_backupCheckBox->isChecked(),
        [this](const stl::TransferProgress& progress) {
            return updateQueueFromProgress(progress);
        });

    setTransferControlsRunning(false);
    if (result.canceled) {
        markRemainingQueueItemsCanceled();
    }

    for (const QString& message : result.messages) {
        appendLog(message);
    }

    appendLog(QStringLiteral("Итог операции: скопировано=%1, пропущено=%2, ошибок=%3%4")
        .arg(result.copied)
        .arg(result.skipped)
        .arg(result.errors)
        .arg(result.canceled ? QStringLiteral(", отменено пользователем") : QString()));

    if (!m_journal.logOperation(m_currentDirection, m_currentPlan, result.copied, result.skipped, result.errors)) {
        appendLog(QStringLiteral("Ошибка записи в SQLite-журнал: %1").arg(m_journal.lastError()));
    }

    refreshModels();
}

void MainWindow::displayQueue(const QVector<stl::PreflightItem>& plan) {
    m_transferQueue = stl::buildQueueFromPlan(plan);
    m_queueTable->setRowCount(m_transferQueue.size());

    for (int row = 0; row < m_transferQueue.size(); ++row) {
        const stl::QueueItem& item = m_transferQueue[row];
        m_queueTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        m_queueTable->setItem(row, 1, new QTableWidgetItem(item.action));
        m_queueTable->setItem(row, 2, new QTableWidgetItem(item.relativePath));
        m_queueTable->setItem(row, 3, new QTableWidgetItem(formatBytes(item.bytes)));
        m_queueTable->setItem(row, 4, new QTableWidgetItem(stl::queueStatusToText(item.status)));
        auto* rowProgress = new QProgressBar(m_queueTable);
        rowProgress->setRange(0, 100);
        rowProgress->setValue(0);
        rowProgress->setFormat(QStringLiteral("%p%"));
        m_queueTable->setCellWidget(row, 5, rowProgress);
        m_queueTable->setItem(row, 6, new QTableWidgetItem(item.message));
    }

    m_totalProgressBar->setValue(0);
    m_queueSummaryLabel->setText(QStringLiteral("В очереди: %1 элементов").arg(m_transferQueue.size()));
}

bool MainWindow::updateQueueFromProgress(const stl::TransferProgress& progress) {
    if (progress.itemIndex >= 0 && progress.itemIndex < m_transferQueue.size()) {
        stl::QueueItem& queueItem = m_transferQueue[progress.itemIndex];
        queueItem.status = progress.status;
        queueItem.message = progress.message;

        int rowProgress = 0;
        if (progress.status == stl::QueueItemStatus::Running) {
            rowProgress = 50;
        } else if (progress.status == stl::QueueItemStatus::Done
                   || progress.status == stl::QueueItemStatus::Skipped
                   || progress.status == stl::QueueItemStatus::Error
                   || progress.status == stl::QueueItemStatus::Canceled) {
            rowProgress = 100;
        }
        queueItem.progressPercent = rowProgress;

        if (m_queueTable->item(progress.itemIndex, 4)) {
            m_queueTable->item(progress.itemIndex, 4)->setText(stl::queueStatusToText(progress.status));
        }
        if (auto* bar = qobject_cast<QProgressBar*>(m_queueTable->cellWidget(progress.itemIndex, 5))) {
            bar->setValue(rowProgress);
        }
        if (m_queueTable->item(progress.itemIndex, 6)) {
            m_queueTable->item(progress.itemIndex, 6)->setText(progress.message);
        }
    }

    updateQueueSummary(progress);
    QApplication::processEvents(QEventLoop::AllEvents, 25);
    return !m_cancelRequested;
}

void MainWindow::updateQueueSummary(const stl::TransferProgress& progress) {
    int percent = 0;
    if (progress.totalBytes > 0) {
        percent = static_cast<int>((progress.processedBytes * 100) / progress.totalBytes);
    } else if (progress.totalItems > 0) {
        percent = static_cast<int>((progress.processedItems * 100) / progress.totalItems);
    }
    percent = qBound(0, percent, 100);
    m_totalProgressBar->setValue(percent);

    m_queueSummaryLabel->setText(QStringLiteral("%1/%2 элементов, %3/%4")
        .arg(progress.processedItems)
        .arg(progress.totalItems)
        .arg(formatBytes(progress.processedBytes))
        .arg(formatBytes(progress.totalBytes)));
}

void MainWindow::markRemainingQueueItemsCanceled() {
    for (int row = 0; row < m_transferQueue.size(); ++row) {
        stl::QueueItem& item = m_transferQueue[row];
        if (item.status == stl::QueueItemStatus::Waiting || item.status == stl::QueueItemStatus::Running) {
            item.status = stl::QueueItemStatus::Canceled;
            item.progressPercent = 100;
            item.message = QStringLiteral("Отменено пользователем");
            if (m_queueTable->item(row, 4)) {
                m_queueTable->item(row, 4)->setText(stl::queueStatusToText(item.status));
            }
            if (auto* bar = qobject_cast<QProgressBar*>(m_queueTable->cellWidget(row, 5))) {
                bar->setValue(100);
            }
            if (m_queueTable->item(row, 6)) {
                m_queueTable->item(row, 6)->setText(item.message);
            }
        }
    }
}

void MainWindow::setTransferControlsRunning(bool running) {
    if (m_executeButton) {
        m_executeButton->setEnabled(!running);
    }
    if (m_cancelButton) {
        m_cancelButton->setEnabled(running);
    }
}

void MainWindow::cancelTransferQueue() {
    m_cancelRequested = true;
    appendLog(QStringLiteral("Получен запрос на отмену очереди. Текущая операция будет остановлена после ближайшего безопасного шага."));
}

QString MainWindow::formatBytes(qint64 bytes) const {
    if (bytes < 0) {
        return QStringLiteral("-");
    }

    static const char* units[] = {"Б", "КБ", "МБ", "ГБ", "ТБ"};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }

    if (unitIndex == 0) {
        return QStringLiteral("%1 %2").arg(bytes).arg(QString::fromUtf8(units[unitIndex]));
    }
    return QStringLiteral("%1 %2").arg(value, 0, 'f', 2).arg(QString::fromUtf8(units[unitIndex]));
}

void MainWindow::refreshModels() {
    setLocalRoot(m_localPathEdit->text());
    refreshRemotePanel();
    appendLog(QStringLiteral("Панели, фильтры и backend-корни обновлены"));
}

void MainWindow::applyFileFilter() {
    applyFilterToModels();
    appendLog(QStringLiteral("Фильтр применён: '%1', режим: %2")
        .arg(m_fileFilterEdit->text().trimmed().isEmpty() ? QStringLiteral("без фильтра") : m_fileFilterEdit->text().trimmed())
        .arg(m_filterModeCombo->currentText()));
}

void MainWindow::clearFileFilter() {
    m_fileFilterEdit->clear();
    m_filterModeCombo->setCurrentIndex(0);
    applyFilterToModels();
    appendLog(QStringLiteral("Фильтр панелей сброшен"));
}

void MainWindow::applyFilterToModels() {
    const QString filterText = m_fileFilterEdit ? m_fileFilterEdit->text() : QString();
    const stl::FileFilterMode mode = filterModeFromUi();

    if (m_localProxyModel) {
        m_localProxyModel->setFilterText(filterText);
        m_localProxyModel->setFilterMode(mode);
    }

    if (m_remoteModel) {
        m_remoteModel->setFilterText(filterText);
        m_remoteModel->setFilterMode(mode);
        if (m_remotePathEdit && !m_remotePathEdit->text().isEmpty()) {
            refreshRemotePanel();
        }
    }
}

stl::FileFilterMode MainWindow::filterModeFromUi() const {
    if (!m_filterModeCombo) {
        return stl::FileFilterMode::All;
    }
    return static_cast<stl::FileFilterMode>(m_filterModeCombo->currentData().toInt());
}

void MainWindow::showHistory() {
    HistoryDialog dialog(m_journal, this);
    dialog.exec();
}

void MainWindow::appendLog(const QString& message) {
    m_log->append(QStringLiteral("[%1] %2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), message));
}
