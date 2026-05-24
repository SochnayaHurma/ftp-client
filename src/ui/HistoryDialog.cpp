#include "HistoryDialog.h"
#include "ui_HistoryDialog.h"

#include <QAbstractItemView>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidgetItem>

HistoryDialog::HistoryDialog(stl::Journal& journal, QWidget* parent)
    : QDialog(parent),
      m_journal(journal) {
    ui = std::make_unique<Ui::HistoryDialog>();
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("История операций SQLite"));
    resize(1100, 720);

    m_operationsTable = ui->operationsTable;
    m_itemsTable = ui->itemsTable;
    m_statusLabel = ui->statusLabel;
    m_exportButton = ui->exportButton;

    m_operationsTable->setColumnCount(8);
    m_operationsTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("Дата"),
        QStringLiteral("Направление"),
        QStringLiteral("Всего"),
        QStringLiteral("Скопировано"),
        QStringLiteral("Пропущено"),
        QStringLiteral("Ошибок"),
        QStringLiteral("Перезаписей")
    });
    m_operationsTable->horizontalHeader()->setStretchLastSection(true);
    m_operationsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_operationsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_operationsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_itemsTable->setColumnCount(8);
    m_itemsTable->setHorizontalHeaderLabels({
        QStringLiteral("Объект"),
        QStringLiteral("Статус"),
        QStringLiteral("Риск"),
        QStringLiteral("Источник"),
        QStringLiteral("Назначение"),
        QStringLiteral("SHA-256 источника"),
        QStringLiteral("SHA-256 назначения"),
        QStringLiteral("Предупреждение")
    });
    m_itemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_itemsTable->horizontalHeader()->setStretchLastSection(true);
    m_itemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_itemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_exportButton->setEnabled(false);
    m_statusLabel->setWordWrap(true);

    connect(ui->refreshButton, &QPushButton::clicked, this, &HistoryDialog::refreshHistory);
    connect(m_exportButton, &QPushButton::clicked, this, &HistoryDialog::exportSelectedOperation);
    connect(ui->closeButton, &QPushButton::clicked, this, &HistoryDialog::accept);
    connect(m_operationsTable, &QTableWidget::itemSelectionChanged, this, &HistoryDialog::onOperationSelected);

    refreshHistory();
}

HistoryDialog::~HistoryDialog() = default;

void HistoryDialog::refreshHistory() {
    const QVector<stl::OperationSummary> operations = m_journal.recentOperations(200);
    fillOperations(operations);
    m_itemsTable->setRowCount(0);
    m_exportButton->setEnabled(false);

    m_statusLabel->setText(QStringLiteral("SQLite: %1. Загружено операций: %2")
        .arg(m_journal.databasePath())
        .arg(operations.size()));
}

void HistoryDialog::fillOperations(const QVector<stl::OperationSummary>& operations) {
    m_operationsTable->setRowCount(operations.size());

    for (int row = 0; row < operations.size(); ++row) {
        const stl::OperationSummary& op = operations[row];
        auto* idItem = new QTableWidgetItem(QString::number(op.id));
        idItem->setData(Qt::UserRole, op.id);
        m_operationsTable->setItem(row, 0, idItem);
        m_operationsTable->setItem(row, 1, new QTableWidgetItem(op.createdAt));
        m_operationsTable->setItem(row, 2, new QTableWidgetItem(op.direction));
        m_operationsTable->setItem(row, 3, new QTableWidgetItem(QString::number(op.filesTotal)));
        m_operationsTable->setItem(row, 4, new QTableWidgetItem(QString::number(op.copied)));
        m_operationsTable->setItem(row, 5, new QTableWidgetItem(QString::number(op.skipped)));
        m_operationsTable->setItem(row, 6, new QTableWidgetItem(QString::number(op.errors)));
        m_operationsTable->setItem(row, 7, new QTableWidgetItem(QString::number(op.overwriteCount)));
    }

    m_operationsTable->resizeColumnsToContents();
}

void HistoryDialog::onOperationSelected() {
    const qint64 operationId = selectedOperationId();
    if (operationId < 0) {
        m_itemsTable->setRowCount(0);
        m_exportButton->setEnabled(false);
        return;
    }

    const QVector<stl::OperationItemRecord> items = m_journal.operationItems(operationId);
    fillItems(items);
    m_exportButton->setEnabled(true);
    m_statusLabel->setText(QStringLiteral("Операция #%1: объектов %2").arg(operationId).arg(items.size()));
}

void HistoryDialog::fillItems(const QVector<stl::OperationItemRecord>& items) {
    m_itemsTable->setRowCount(items.size());

    auto shortHash = [](const QString& hash) {
        return hash.isEmpty() ? QStringLiteral("-") : hash.left(16) + QStringLiteral("...");
    };

    for (int row = 0; row < items.size(); ++row) {
        const stl::OperationItemRecord& item = items[row];
        m_itemsTable->setItem(row, 0, new QTableWidgetItem(item.relativePath));
        m_itemsTable->setItem(row, 1, new QTableWidgetItem(item.status));
        m_itemsTable->setItem(row, 2, new QTableWidgetItem(item.risk));
        m_itemsTable->setItem(row, 3, new QTableWidgetItem(item.sourcePath));
        m_itemsTable->setItem(row, 4, new QTableWidgetItem(item.destinationPath));
        m_itemsTable->setItem(row, 5, new QTableWidgetItem(shortHash(item.sourceSha256)));
        m_itemsTable->setItem(row, 6, new QTableWidgetItem(shortHash(item.destinationSha256)));
        m_itemsTable->setItem(row, 7, new QTableWidgetItem(item.warning));
    }

    m_itemsTable->resizeColumnsToContents();
}

void HistoryDialog::exportSelectedOperation() {
    const qint64 operationId = selectedOperationId();
    if (operationId < 0) {
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Экспорт операции в CSV"),
        QStringLiteral("operation_%1.csv").arg(operationId),
        QStringLiteral("CSV (*.csv)"));

    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Экспорт"), QStringLiteral("Не удалось открыть файл для записи."));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "relative_path;status;risk;source_path;destination_path;source_sha256;destination_sha256;warning\n";

    const QVector<stl::OperationItemRecord> items = m_journal.operationItems(operationId);
    for (const stl::OperationItemRecord& item : items) {
        out << csvEscape(item.relativePath) << ';'
            << csvEscape(item.status) << ';'
            << csvEscape(item.risk) << ';'
            << csvEscape(item.sourcePath) << ';'
            << csvEscape(item.destinationPath) << ';'
            << csvEscape(item.sourceSha256) << ';'
            << csvEscape(item.destinationSha256) << ';'
            << csvEscape(item.warning) << '\n';
    }

    QMessageBox::information(this, QStringLiteral("Экспорт"), QStringLiteral("Операция #%1 экспортирована.").arg(operationId));
}

QString HistoryDialog::csvEscape(const QString& value) const {
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

qint64 HistoryDialog::selectedOperationId() const {
    const QModelIndexList rows = m_operationsTable->selectionModel()->selectedRows(0);
    if (rows.isEmpty()) {
        return -1;
    }
    return m_operationsTable->item(rows.first().row(), 0)->data(Qt::UserRole).toLongLong();
}
