#include <QtTest/QtTest>

#include "core/PreflightAnalyzer.h"
#include "core/storage/LocalStorageBackend.h"

#include <QDir>
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

const PreflightItem* findItem(const QVector<PreflightItem>& plan, const QString& relativePath) {
    for (const PreflightItem& item : plan) {
        if (item.relativePath == relativePath) {
            return &item;
        }
    }
    return nullptr;
}

} // namespace

class TestPreflightAnalyzer : public QObject {
    Q_OBJECT

private slots:
    void detectsNewFile();
    void detectsSameFileBySha256();
    void detectsOverwriteBySha256();
    void detectsDirectoryRecursively();
    void detectsFileDirectoryConflict();
    void reportsMissingSourceAsError();
};

void TestPreflightAnalyzer::detectsNewFile() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString sourceFile = source.absolutePath(QStringLiteral("docs/readme.txt"));
    QVERIFY(writeTextFile(sourceFile, QStringLiteral("hello from source")));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({sourceFile}, source, destination);

    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan[0].relativePath, QStringLiteral("docs/readme.txt"));
    QCOMPARE(plan[0].status, PreflightStatus::NewFile);
    QCOMPARE(plan[0].risk, RiskLevel::Low);
    QVERIFY(!plan[0].sourceSha256.isEmpty());
}

void TestPreflightAnalyzer::detectsSameFileBySha256() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString sourceFile = source.absolutePath(QStringLiteral("sync/same.txt"));
    const QString destinationFile = destination.absolutePath(QStringLiteral("sync/same.txt"));
    QVERIFY(writeTextFile(sourceFile, QStringLiteral("identical payload")));
    QVERIFY(writeTextFile(destinationFile, QStringLiteral("identical payload")));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({sourceFile}, source, destination);

    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan[0].status, PreflightStatus::Same);
    QCOMPARE(plan[0].risk, RiskLevel::None);
    QCOMPARE(plan[0].sourceSha256, plan[0].destinationSha256);
}

void TestPreflightAnalyzer::detectsOverwriteBySha256() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString sourceFile = source.absolutePath(QStringLiteral("sync/overwrite.txt"));
    const QString destinationFile = destination.absolutePath(QStringLiteral("sync/overwrite.txt"));
    QVERIFY(writeTextFile(sourceFile, QStringLiteral("new payload")));
    QVERIFY(writeTextFile(destinationFile, QStringLiteral("old payload")));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({sourceFile}, source, destination);

    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan[0].status, PreflightStatus::Overwrite);
    QVERIFY(plan[0].risk == RiskLevel::Low || plan[0].risk == RiskLevel::Medium || plan[0].risk == RiskLevel::High);
    QVERIFY(plan[0].sourceSha256 != plan[0].destinationSha256);
    QVERIFY(plan[0].warning.contains(QStringLiteral("SHA-256")));
}

void TestPreflightAnalyzer::detectsDirectoryRecursively() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString root = source.absolutePath(QStringLiteral("project"));
    QVERIFY(QDir().mkpath(root));
    QVERIFY(writeTextFile(source.absolutePath(QStringLiteral("project/README.md")), QStringLiteral("readme")));
    QVERIFY(writeTextFile(source.absolutePath(QStringLiteral("project/src/main.cpp")), QStringLiteral("int main(){}")));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({root}, source, destination);

    QVERIFY(plan.size() >= 3);
    QVERIFY(findItem(plan, QStringLiteral("project")) != nullptr);
    QVERIFY(findItem(plan, QStringLiteral("project/README.md")) != nullptr);
    QVERIFY(findItem(plan, QStringLiteral("project/src/main.cpp")) != nullptr);
    QCOMPARE(findItem(plan, QStringLiteral("project/README.md"))->status, PreflightStatus::NewFile);
}

void TestPreflightAnalyzer::detectsFileDirectoryConflict() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString sourceFile = source.absolutePath(QStringLiteral("conflict/item"));
    const QString destinationDirectory = destination.absolutePath(QStringLiteral("conflict/item"));
    QVERIFY(writeTextFile(sourceFile, QStringLiteral("file content")));
    QVERIFY(QDir().mkpath(destinationDirectory));

    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({sourceFile}, source, destination);

    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan[0].status, PreflightStatus::Conflict);
    QCOMPARE(plan[0].risk, RiskLevel::High);
    QVERIFY(plan[0].warning.contains(QStringLiteral("папка")));
}

void TestPreflightAnalyzer::reportsMissingSourceAsError() {
    QTemporaryDir sourceDir;
    QTemporaryDir destinationDir;
    QVERIFY(sourceDir.isValid());
    QVERIFY(destinationDir.isValid());

    LocalStorageBackend source(sourceDir.path());
    LocalStorageBackend destination(destinationDir.path());

    const QString missing = source.absolutePath(QStringLiteral("missing.txt"));
    const QVector<PreflightItem> plan = PreflightAnalyzer().analyze({missing}, source, destination);

    QCOMPARE(plan.size(), 1);
    QCOMPARE(plan[0].status, PreflightStatus::Error);
    QCOMPARE(plan[0].risk, RiskLevel::High);
}

QTEST_MAIN(TestPreflightAnalyzer)
#include "TestPreflightAnalyzer.moc"
