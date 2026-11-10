// SPDX-License-Identifier: MIT
//
// Tests for Daqster::DependencyManager (REQ-SW-PL-088). No network: the HTTPS
// success path is covered through `adopt()`; `install()` is exercised only for
// its synchronous validation (non-https / missing checksum), which must fail
// before any request or partial file.

#include "test_dependency_manager.h"

#include "Dependencies/DependencyManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

QString sha256Hex(const QByteArray& data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QString sha256OfFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return sha256Hex(file.readAll());
}

bool writeFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(content) == content.size();
}

QJsonObject specJson(const QString& id, const QString& kind,
                     const QString& version, const QString& url,
                     const QString& sha256, const QString& file)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("kind"), kind);
    object.insert(QStringLiteral("version"), version);
    if (!url.isEmpty())
        object.insert(QStringLiteral("url"), url);
    if (!sha256.isEmpty())
        object.insert(QStringLiteral("sha256"), sha256);
    object.insert(QStringLiteral("file"), file);
    return object;
}

QString writeManifest(const QString& dir, const QJsonArray& dependencies,
                      int version = 1)
{
    QJsonObject root;
    root.insert(QStringLiteral("version"), version);
    root.insert(QStringLiteral("dependencies"), dependencies);
    const QString path = QDir(dir).filePath(QStringLiteral("dependencies.json"));
    writeFile(path, QJsonDocument(root).toJson());
    return path;
}

Daqster::DependencySpec localSpec(const QString& root, const QByteArray& data)
{
    Daqster::DependencySpec spec;
    spec.id = QStringLiteral("tool");
    spec.kind = QStringLiteral("binary");
    spec.version = QStringLiteral("1.0");
    spec.sha256 = sha256Hex(data);
    spec.size = data.size();
    spec.file = QStringLiteral("tool");
    Q_UNUSED(root);
    return spec;
}

} // namespace

void TestDependencyManager::manifestParses()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QJsonObject object = specJson(
        QStringLiteral("tool"), QStringLiteral("binary"),
        QStringLiteral("1.2.3"), QStringLiteral("https://example.com/tool"),
        QString(64, QLatin1Char('a')), QStringLiteral("tool"));
    object.insert(QStringLiteral("size"), 1234);
    object.insert(QStringLiteral("provides"),
                  QJsonArray{ QStringLiteral("binaries/tool") });
    object.insert(QStringLiteral("executable"), true);

    const QString path = writeManifest(dir.path(), QJsonArray{ object });
    Daqster::DependencyManager manager(dir.path());
    QString error;
    QVERIFY2(manager.loadManifest(path, &error), qPrintable(error));
    QCOMPARE(manager.specs().size(), 1);

    const Daqster::DependencySpec spec = manager.specs().first();
    QCOMPARE(spec.id, QStringLiteral("tool"));
    QCOMPARE(spec.kind, QStringLiteral("binary"));
    QCOMPARE(spec.version, QStringLiteral("1.2.3"));
    QCOMPARE(spec.url, QStringLiteral("https://example.com/tool"));
    QCOMPARE(spec.sha256, QString(64, QLatin1Char('a')));
    QCOMPARE(spec.size, qint64(1234));
    QCOMPARE(spec.file, QStringLiteral("tool"));
    QCOMPARE(spec.provides, QStringList{ QStringLiteral("binaries/tool") });
    QVERIFY(spec.executable);
}

void TestDependencyManager::manifestRejectsNonHttps()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Daqster::DependencyManager manager(dir.path());

    const QString sha(64, QLatin1Char('a'));
    const QStringList badUrls{
        QStringLiteral("http://example.com/tool"),
        QStringLiteral("file:///tmp/tool"),
        QStringLiteral("/tmp/tool"),
    };
    for (const QString& url : badUrls) {
        const QString path = writeManifest(
            dir.path(),
            QJsonArray{ specJson(QStringLiteral("tool"),
                                 QStringLiteral("binary"),
                                 QStringLiteral("1.0"), url, sha,
                                 QStringLiteral("tool")) });
        QString error;
        QVERIFY2(!manager.loadManifest(path, &error), qPrintable(url));
        QVERIFY2(error.contains(QStringLiteral("https")), qPrintable(error));
    }
}

void TestDependencyManager::manifestRejectsMissingChecksum()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Daqster::DependencyManager manager(dir.path());

    const QString path = writeManifest(
        dir.path(),
        QJsonArray{ specJson(QStringLiteral("tool"), QStringLiteral("binary"),
                             QStringLiteral("1.0"),
                             QStringLiteral("https://example.com/tool"),
                             QString(), QStringLiteral("tool")) });
    QString error;
    QVERIFY(!manager.loadManifest(path, &error));
    QVERIFY2(error.contains(QStringLiteral("sha256")), qPrintable(error));
}

void TestDependencyManager::manifestRejectsDuplicateIdAndUnknownKind()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Daqster::DependencyManager manager(dir.path());
    const QString sha(64, QLatin1Char('a'));

    const QJsonObject first = specJson(
        QStringLiteral("tool"), QStringLiteral("binary"),
        QStringLiteral("1.0"), QStringLiteral("https://example.com/tool"), sha,
        QStringLiteral("tool"));
    QString path = writeManifest(dir.path(), QJsonArray{ first, first });
    QString error;
    QVERIFY(!manager.loadManifest(path, &error));
    QVERIFY2(error.contains(QStringLiteral("duplicate")), qPrintable(error));

    const QJsonObject wrongKind = specJson(
        QStringLiteral("tool"), QStringLiteral("banana"),
        QStringLiteral("1.0"), QStringLiteral("https://example.com/tool"), sha,
        QStringLiteral("tool"));
    path = writeManifest(dir.path(), QJsonArray{ wrongKind });
    QVERIFY(!manager.loadManifest(path, &error));
    QVERIFY2(error.contains(QStringLiteral("kind")), qPrintable(error));
}

void TestDependencyManager::manifestRejectsMalformedSha256()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Daqster::DependencyManager manager(dir.path());

    const QStringList badShas{
        QString(63, QLatin1Char('a')),
        QString(64, QLatin1Char('A')),
        QString(64, QLatin1Char('z')),
        QStringLiteral("8642541b2"),
    };
    for (const QString& sha : badShas) {
        const QString path = writeManifest(
            dir.path(),
            QJsonArray{ specJson(QStringLiteral("tool"),
                                 QStringLiteral("binary"),
                                 QStringLiteral("1.0"),
                                 QStringLiteral("https://example.com/tool"),
                                 sha, QStringLiteral("tool")) });
        QString error;
        QVERIFY2(!manager.loadManifest(path, &error), qPrintable(sha));
        QVERIFY2(error.contains(QStringLiteral("sha256")), qPrintable(error));
    }
}

void TestDependencyManager::statusAcceptsCorrectChecksum()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray data("payload");
    const Daqster::DependencySpec spec = localSpec(root.path(), data);
    const QString installed = QDir(root.path()).filePath(
        QStringLiteral("tool/1.0/tool"));
    QVERIFY(writeFile(installed, data));

    Daqster::DependencyManager manager(root.path());
    const Daqster::DependencyStatus status = manager.status(spec);
    QVERIFY2(status.present, qPrintable(status.error));
    QVERIFY(status.error.isEmpty());
    QCOMPARE(status.path, installed);
}

void TestDependencyManager::statusDetectsChecksumMismatch()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const Daqster::DependencySpec spec =
        localSpec(root.path(), QByteArray("expected"));
    QVERIFY(writeFile(
        QDir(root.path()).filePath(QStringLiteral("tool/1.0/tool")),
        QByteArray("tampered")));

    Daqster::DependencyManager manager(root.path());
    const Daqster::DependencyStatus status = manager.status(spec);
    QVERIFY(!status.present);
    QVERIFY2(status.error.contains(spec.id), qPrintable(status.error));
    QVERIFY2(status.error.contains(QStringLiteral("sha256")),
             qPrintable(status.error));
}

void TestDependencyManager::statusMissingNamesId()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const Daqster::DependencySpec spec =
        localSpec(root.path(), QByteArray("payload"));

    Daqster::DependencyManager manager(root.path());
    const Daqster::DependencyStatus status = manager.status(spec);
    QVERIFY(!status.present);
    QVERIFY2(status.error.contains(QStringLiteral("tool")),
             qPrintable(status.error));
    QVERIFY2(status.error.contains(status.path), qPrintable(status.error));
}

void TestDependencyManager::adoptRecordsLockWithoutCopying()
{
    QTemporaryDir root;
    QTemporaryDir source;
    QVERIFY(root.isValid() && source.isValid());
    const QByteArray data("existing artifact");
    const QString existing =
        QDir(source.path()).filePath(QStringLiteral("preexisting"));
    QVERIFY(writeFile(existing, data));

    Daqster::DependencySpec spec = localSpec(root.path(), data);
    spec.url = QStringLiteral("https://example.com/tool");

    Daqster::DependencyManager manager(root.path());
    QString error;
    QVERIFY2(manager.adopt(spec, existing, &error), qPrintable(error));

    QVERIFY(QFileInfo::exists(manager.lockFilePath()));
    const QJsonArray entries =
        manager.lock().value(QStringLiteral("dependencies")).toArray();
    QCOMPARE(entries.size(), 1);
    const QJsonObject entry = entries.first().toObject();
    QCOMPARE(entry.value(QStringLiteral("id")).toString(), spec.id);
    QCOMPARE(entry.value(QStringLiteral("version")).toString(), spec.version);
    QCOMPARE(entry.value(QStringLiteral("path")).toString(),
             QFileInfo(existing).absoluteFilePath());
    QCOMPARE(entry.value(QStringLiteral("sha256")).toString(), spec.sha256);
    QCOMPARE(entry.value(QStringLiteral("adopted")).toBool(), true);

    // Adopt never copies: the install layout stays empty …
    QVERIFY(!QFileInfo::exists(
        QDir(root.path()).filePath(QStringLiteral("tool/1.0/tool"))));
    // … and status follows the recorded path.
    QVERIFY(manager.status(spec).present);
}

void TestDependencyManager::adoptRejectsChecksumMismatch()
{
    QTemporaryDir root;
    QTemporaryDir source;
    QVERIFY(root.isValid() && source.isValid());
    const QString existing =
        QDir(source.path()).filePath(QStringLiteral("preexisting"));
    QVERIFY(writeFile(existing, QByteArray("actual")));

    Daqster::DependencySpec spec =
        localSpec(root.path(), QByteArray("expected"));

    Daqster::DependencyManager manager(root.path());
    QString error;
    QVERIFY(!manager.adopt(spec, existing, &error));
    QVERIFY2(error.contains(QStringLiteral("sha256")), qPrintable(error));
    QCOMPARE(manager.lock()
                 .value(QStringLiteral("dependencies"))
                 .toArray()
                 .size(),
             0);
    QVERIFY(!QFileInfo::exists(manager.lockFilePath()));
}

void TestDependencyManager::adoptOnlyRecordsComputedChecksum()
{
    QTemporaryDir root;
    QTemporaryDir source;
    QVERIFY(root.isValid() && source.isValid());
    const QByteArray data("adopt only");
    const QString existing =
        QDir(source.path()).filePath(QStringLiteral("preexisting"));
    QVERIFY(writeFile(existing, data));

    Daqster::DependencySpec spec; // no url, no sha256
    spec.id = QStringLiteral("model/x");
    spec.kind = QStringLiteral("model");
    spec.version = QStringLiteral("2026-01");
    spec.file = QStringLiteral("x.gguf");

    Daqster::DependencyManager manager(root.path());
    QString error;
    QVERIFY2(manager.adopt(spec, existing, &error), qPrintable(error));

    const QJsonObject entry = manager.lock()
                                  .value(QStringLiteral("dependencies"))
                                  .toArray()
                                  .first()
                                  .toObject();
    QCOMPARE(entry.value(QStringLiteral("sha256")).toString(),
             sha256Hex(data));
    QVERIFY(manager.status(spec).present);
}

void TestDependencyManager::installRejectsNonHttps()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    Daqster::DependencyManager manager(root.path());

    const QStringList badUrls{
        QStringLiteral("http://127.0.0.1:1/tool"),
        QStringLiteral("file:///tmp/tool"),
    };
    for (const QString& url : badUrls) {
        Daqster::DependencySpec spec =
            localSpec(root.path(), QByteArray("payload"));
        spec.url = url;
        bool called = false;
        bool ok = true;
        QString error;
        manager.install(spec, [&](bool success, QString reason) {
            called = true;
            ok = success;
            error = reason;
        });
        QVERIFY2(called, qPrintable(url));
        QVERIFY(!ok);
        QVERIFY2(error.contains(QStringLiteral("https")), qPrintable(error));
        QVERIFY2(!QFileInfo::exists(
                     QDir(root.path()).filePath(QStringLiteral("tool/1.0/tool"))),
                 "a rejected URL must not create a partial file");
    }
}

void TestDependencyManager::installRejectsMissingChecksum()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    Daqster::DependencyManager manager(root.path());

    Daqster::DependencySpec spec;
    spec.id = QStringLiteral("tool");
    spec.kind = QStringLiteral("binary");
    spec.version = QStringLiteral("1.0");
    spec.url = QStringLiteral("https://example.com/tool");
    spec.file = QStringLiteral("tool");

    bool called = false;
    bool ok = true;
    QString error;
    manager.install(spec, [&](bool success, QString reason) {
        called = true;
        ok = success;
        error = reason;
    });
    QVERIFY(called);
    QVERIFY(!ok);
    QVERIFY2(error.contains(QStringLiteral("sha256")), qPrintable(error));
}

void TestDependencyManager::lockRoundTripAcrossInstances()
{
    QTemporaryDir root;
    QTemporaryDir source;
    QVERIFY(root.isValid() && source.isValid());
    const QByteArray data("round trip");
    const QString existing =
        QDir(source.path()).filePath(QStringLiteral("artifact"));
    QVERIFY(writeFile(existing, data));

    const Daqster::DependencySpec spec =
        localSpec(root.path(), data);

    {
        Daqster::DependencyManager writer(root.path());
        QString error;
        QVERIFY2(writer.adopt(spec, existing, &error), qPrintable(error));
    }

    Daqster::DependencyManager reader(root.path());
    const QJsonArray entries =
        reader.lock().value(QStringLiteral("dependencies")).toArray();
    QCOMPARE(entries.size(), 1);
    const QJsonObject entry = entries.first().toObject();
    QCOMPARE(entry.value(QStringLiteral("id")).toString(), spec.id);
    QCOMPARE(entry.value(QStringLiteral("adopted")).toBool(), true);
    QCOMPARE(entry.value(QStringLiteral("path")).toString(),
             QFileInfo(existing).absoluteFilePath());
    QVERIFY(reader.status(spec).present);
}
