#pragma once

#include "../core/PreflightAnalyzer.h"
#include "../core/TransferManager.h"
#include "../core/TransferQueue.h"
#include "../core/Journal.h"
#include "../core/storage/ConnectionProfileStore.h"
#include "../core/storage/CurlRemoteStorage.h"
#include "../core/storage/LocalStorageBackend.h"
#include "../core/storage/RemoteConnectionProfile.h"
#include "FileFilterMode.h"
#include "FileSystemFilterProxyModel.h"
#include "RemoteFileModel.h"

#include <memory>

#include <QCheckBox>
#include <QComboBox>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QModelIndex>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeView>

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void chooseLocalRoot();
    void chooseRemoteRoot();
    void configureRemoteConnection();
    void switchToEmulator();
    void remoteGoUp();
    void onRemoteDoubleClicked(const QModelIndex& index);
    void analyzeUpload();
    void analyzeDownload();
    void executeCurrentPlan();
    void cancelTransferQueue();
    void refreshModels();
    void applyFileFilter();
    void clearFileFilter();
    void showHistory();

private:
    void buildUi();
    void configureLocalModel(QFileSystemModel* model, QTreeView* view, const QString& rootPath);
    QStringList selectedLocalPaths() const;
    QStringList selectedRemotePaths() const;
    void analyze(stl::TransferDirection direction);
    void displayPlan(const QVector<stl::PreflightItem>& plan);
    void colorizePlanRow(int row, const stl::PreflightItem& item);
    void displayQueue(const QVector<stl::PreflightItem>& plan);
    bool updateQueueFromProgress(const stl::TransferProgress& progress);
    void updateQueueSummary(const stl::TransferProgress& progress);
    void markRemainingQueueItemsCanceled();
    void setTransferControlsRunning(bool running);
    QString formatBytes(qint64 bytes) const;
    void appendLog(const QString& message);
    bool currentSelectionStillMatchesPlan() const;
    void clearCurrentPlan(const QString& reason = QString());
    void setLocalRoot(const QString& path);
    void setRemoteEmulatorRoot(const QString& path);
    void refreshRemotePanel();
    void updateBackendRoots();
    void applyFilterToModels();
    stl::FileFilterMode filterModeFromUi() const;
    QString defaultRemoteRoot() const;
    const stl::IStorageBackend& activeRemoteBackend() const;
    stl::IStorageBackend& activeRemoteBackend();
    QString activeRemoteDescription() const;
    bool preserveSourceHierarchyFromUi() const;

    std::unique_ptr<Ui::MainWindow> ui;

    QLineEdit* m_localPathEdit = nullptr;
    QLineEdit* m_remotePathEdit = nullptr;
    QLabel* m_remoteBackendLabel = nullptr;
    QLineEdit* m_fileFilterEdit = nullptr;
    QComboBox* m_filterModeCombo = nullptr;
    QTreeView* m_localView = nullptr;
    QTreeView* m_remoteView = nullptr;
    QFileSystemModel* m_localModel = nullptr;
    FileSystemFilterProxyModel* m_localProxyModel = nullptr;
    RemoteFileModel* m_remoteModel = nullptr;
    QTableWidget* m_planTable = nullptr;
    QTableWidget* m_queueTable = nullptr;
    QTextEdit* m_log = nullptr;
    QCheckBox* m_backupCheckBox = nullptr;
    QCheckBox* m_preserveSourceHierarchyCheckBox = nullptr;
    QProgressBar* m_totalProgressBar = nullptr;
    QLabel* m_queueSummaryLabel = nullptr;
    QPushButton* m_executeButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    stl::LocalStorageBackend m_localBackend;
    stl::LocalStorageBackend m_remoteEmulatorBackend;
    std::unique_ptr<stl::CurlRemoteStorage> m_curlRemoteBackend;
    bool m_useCurlRemoteBackend = false;

    stl::RemoteConnectionProfile m_remoteProfile;
    stl::ConnectionProfileStore m_profileStore;
    stl::PreflightAnalyzer m_analyzer;
    stl::TransferManager m_transferManager;
    stl::Journal m_journal;
    QVector<stl::PreflightItem> m_currentPlan;
    QStringList m_analyzedSelectionPaths;
    QVector<stl::QueueItem> m_transferQueue;
    stl::TransferDirection m_currentDirection = stl::TransferDirection::Upload;
    bool m_cancelRequested = false;
};
