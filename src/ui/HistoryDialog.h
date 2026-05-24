#pragma once

#include "../core/Journal.h"

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <memory>

namespace Ui { class HistoryDialog; }

class HistoryDialog final : public QDialog {
    Q_OBJECT

public:
    explicit HistoryDialog(stl::Journal& journal, QWidget* parent = nullptr);
    ~HistoryDialog() override;

private slots:
    void refreshHistory();
    void onOperationSelected();
    void exportSelectedOperation();

private:
    QString csvEscape(const QString& value) const;
    qint64 selectedOperationId() const;
    void fillOperations(const QVector<stl::OperationSummary>& operations);
    void fillItems(const QVector<stl::OperationItemRecord>& items);

    stl::Journal& m_journal;
    std::unique_ptr<Ui::HistoryDialog> ui;
    QTableWidget* m_operationsTable = nullptr;
    QTableWidget* m_itemsTable = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_exportButton = nullptr;
};
