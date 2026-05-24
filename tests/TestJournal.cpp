#include <QtTest/QtTest>

#include "core/Journal.h"

#include <QTemporaryDir>

using namespace stl;

class TestJournal : public QObject {
    Q_OBJECT

private slots:
    void writesAndReadsOperationHistory();
};

void TestJournal::writesAndReadsOperationHistory() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Journal journal(tempDir.filePath(QStringLiteral("journal.sqlite")));
    QVERIFY2(journal.open(), journal.lastError().toUtf8().constData());

    PreflightItem newFile;
    newFile.relativePath = QStringLiteral("docs/new.txt");
    newFile.sourcePath = QStringLiteral("/source/docs/new.txt");
    newFile.destinationPath = QStringLiteral("/destination/docs/new.txt");
    newFile.status = PreflightStatus::NewFile;
    newFile.risk = RiskLevel::Low;
    newFile.sourceSha256 = QStringLiteral("111");

    PreflightItem overwrite;
    overwrite.relativePath = QStringLiteral("docs/overwrite.txt");
    overwrite.sourcePath = QStringLiteral("/source/docs/overwrite.txt");
    overwrite.destinationPath = QStringLiteral("/destination/docs/overwrite.txt");
    overwrite.status = PreflightStatus::Overwrite;
    overwrite.risk = RiskLevel::High;
    overwrite.sourceSha256 = QStringLiteral("222");
    overwrite.destinationSha256 = QStringLiteral("333");
    overwrite.warning = QStringLiteral("Будет перезаписан");

    QVERIFY2(journal.logOperation(TransferDirection::Upload, {newFile, overwrite}, 2, 0, 0),
             journal.lastError().toUtf8().constData());

    const QVector<OperationSummary> operations = journal.recentOperations();
    QCOMPARE(operations.size(), 1);
    QCOMPARE(operations[0].filesTotal, 2);
    QCOMPARE(operations[0].copied, 2);
    QCOMPARE(operations[0].overwriteCount, 1);

    const QVector<OperationItemRecord> items = journal.operationItems(operations[0].id);
    QCOMPARE(items.size(), 2);
    QCOMPARE(items[0].relativePath, QStringLiteral("docs/new.txt"));
    QCOMPARE(items[1].relativePath, QStringLiteral("docs/overwrite.txt"));
    QCOMPARE(items[1].risk, riskToText(RiskLevel::High));
}

QTEST_MAIN(TestJournal)
#include "TestJournal.moc"
