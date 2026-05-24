#include <QtTest/QtTest>

#include "core/PreflightAnalyzer.h"
#include "core/TransferManager.h"
#include "core/storage/LocalStorageBackend.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

using namespace stl;

namespace {

bool writeTextFile(const QString& path, const QString& text) {
    QDir dir;
    if (!dir.mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(text.toUtf8());
    return true;
}

QString readTextFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

bool backupContains(const QString& destinationRoot, const QString& suffix, const QString& expectedText) {
    QDirIterator it(QDir(destinationRoot).filePath(QStringLiteral(".stl_backup")), QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).isFile() && path.endsWith(suffix)) {
            if (readTextFile(path) == expectedText) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

class TestTransferManager : public QObject {
    Q_OBJECT

private slots:
    void copiesNewFile();
    void skipsSameFile();
    void overwritesWithBackup();
    void blocksConflict();
    void reportsProgressAndCanCancel();
};

void TestTransferManager::copiesNewFile() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString sourceFile = source.absolutePath(QStringLiteral("docs/new.txt"));
    const QString destinationFile = destination.absolutePath(QStringLiteral("docs/new.txt"));
    QVERIFY(writeTextFile(sourceFile, QStringLiteral("new file")));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({sourceFile}, source, destination);
    TransferResult result = TransferManager().execute(plan, source, destination, true);

    QCOMPARE(result.copied, 1);
    QCOMPARE(result.skipped, 0);
    QCOMPARE(result.errors, 0);
    QCOMPARE(readTextFile(destinationFile), QStringLiteral("new file"));
}

void TestTransferManager::skipsSameFile() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString sourceFile = source.absolutePath(QStringLiteral("same.txt"));
    const QString destinationFile = destination.absolutePath(QStringLiteral("same.txt"));
    QVERIFY(writeTextFile(sourceFile, QStringLiteral("same")));
    QVERIFY(writeTextFile(destinationFile, QStringLiteral("same")));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({sourceFile}, source, destination);
    TransferResult result = TransferManager().execute(plan, source, destination, true);

    QCOMPARE(result.copied, 0);
    QCOMPARE(result.skipped, 1);
    QCOMPARE(result.errors, 0);
    QCOMPARE(readTextFile(destinationFile), QStringLiteral("same"));
}

void TestTransferManager::overwritesWithBackup() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString sourceFile = source.absolutePath(QStringLiteral("docs/data.txt"));
    const QString destinationFile = destination.absolutePath(QStringLiteral("docs/data.txt"));
    QVERIFY(writeTextFile(sourceFile, QStringLiteral("new version")));
    QVERIFY(writeTextFile(destinationFile, QStringLiteral("old version")));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({sourceFile}, source, destination);
    TransferResult result = TransferManager().execute(plan, source, destination, true);

    QCOMPARE(result.copied, 1);
    QCOMPARE(result.errors, 0);
    QCOMPARE(readTextFile(destinationFile), QStringLiteral("new version"));
    QVERIFY(backupContains(destination.root(), QStringLiteral("docs/data.txt"), QStringLiteral("old version")));
}

void TestTransferManager::blocksConflict() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString sourceFile = source.absolutePath(QStringLiteral("conflict/item"));
    QVERIFY(writeTextFile(sourceFile, QStringLiteral("payload")));
    QVERIFY(QDir().mkpath(destination.absolutePath(QStringLiteral("conflict/item"))));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({sourceFile}, source, destination);
    TransferResult result = TransferManager().execute(plan, source, destination, true);

    QCOMPARE(result.copied, 0);
    QCOMPARE(result.errors, 1);
    QVERIFY(QFileInfo(destination.absolutePath(QStringLiteral("conflict/item"))).isDir());
}

void TestTransferManager::reportsProgressAndCanCancel() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString first = source.absolutePath(QStringLiteral("a.txt"));
    const QString second = source.absolutePath(QStringLiteral("b.txt"));
    QVERIFY(writeTextFile(first, QStringLiteral("a")));
    QVERIFY(writeTextFile(second, QStringLiteral("b")));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({first, second}, source, destination);
    int callbacks = 0;
    TransferResult result = TransferManager().execute(plan, source, destination, false,
        [&](const TransferProgress& progress) {
            Q_UNUSED(progress)
            callbacks++;
            return callbacks < 3;
        });

    QVERIFY(callbacks >= 3);
    QVERIFY(result.canceled);
}

QTEST_MAIN(TestTransferManager)
#include "TestTransferManager.moc"
