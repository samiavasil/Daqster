#include "Dependencies/DependencyManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QUrl>

#include <memory>

Q_LOGGING_CATEGORY(lcDependencies, "daqster.dependencies")

namespace Daqster {
namespace {

const char* const kKinds[] = { "binary", "library", "model", "voice", "source" };

bool kindIsKnown(const QString& kind)
{
    for (const char* candidate : kKinds) {
        if (kind == QLatin1String(candidate))
            return true;
    }
    return false;
}

/// A path component that cannot escape its parent directory.
bool componentProblem(const QString& value, QString* why)
{
    if (value.isEmpty()) {
        *why = QStringLiteral("path component is empty");
        return true;
    }
    if (value.contains(QLatin1Char('\\'))) {
        *why = QStringLiteral("\"%1\" contains a backslash").arg(value);
        return true;
    }
    if (value.contains(QLatin1Char(':'))) {
        *why = QStringLiteral("\"%1\" looks like a drive/stream path").arg(value);
        return true;
    }
    const QStringList parts = value.split(QLatin1Char('/'));
    for (const QString& part : parts) {
        if (part.isEmpty() || part == QLatin1String(".")
            || part == QLatin1String("..")) {
            *why = QStringLiteral("\"%1\" has an empty, '.' or '..' segment")
                       .arg(value);
            return true;
        }
    }
    return false;
}

bool isValidSha256(const QString& value)
{
    if (value.size() != 64)
        return false;
    for (const QChar ch : value) {
        const bool digit = ch >= QLatin1Char('0') && ch <= QLatin1Char('9');
        const bool lower = ch >= QLatin1Char('a') && ch <= QLatin1Char('f');
        if (!digit && !lower)
            return false;
    }
    return true;
}

bool urlProblem(const QString& url, QString* why)
{
    const QUrl parsed(url, QUrl::StrictMode);
    if (!parsed.isValid() || parsed.host().isEmpty()) {
        *why = QStringLiteral("invalid URL \"%1\" — expected https://<host>/<path>")
                   .arg(url);
        return true;
    }
    if (parsed.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) != 0) {
        *why = QStringLiteral("only https:// URLs are allowed, got \"%1\"")
                   .arg(parsed.scheme());
        return true;
    }
    return false;
}

/// Full structural validation; used on manifest load and before every action.
bool specProblem(const DependencySpec& spec, QString* why)
{
    if (spec.id.isEmpty()) {
        *why = QStringLiteral("id is empty");
        return true;
    }
    if (componentProblem(spec.id, why))
        return true;
    if (!kindIsKnown(spec.kind)) {
        *why = QStringLiteral("'%1': unknown kind \"%2\" (expected binary, "
                              "library, model, voice or source)")
                   .arg(spec.id, spec.kind);
        return true;
    }
    if (spec.version.isEmpty()) {
        *why = QStringLiteral("'%1': version is empty").arg(spec.id);
        return true;
    }
    if (componentProblem(spec.version, why))
        return true;
    if (spec.file.isEmpty()) {
        *why = QStringLiteral("'%1': file is empty").arg(spec.id);
        return true;
    }
    if (componentProblem(spec.file, why))
        return true;
    if (spec.size < 0) {
        *why = QStringLiteral("'%1': size must not be negative").arg(spec.id);
        return true;
    }
    if (!spec.url.isEmpty()) {
        if (urlProblem(spec.url, why))
            return true;
        if (spec.sha256.isEmpty()) {
            *why = QStringLiteral("'%1': sha256 is required for a download")
                       .arg(spec.id);
            return true;
        }
    }
    if (!spec.sha256.isEmpty() && !isValidSha256(spec.sha256)) {
        *why = QStringLiteral("'%1': sha256 must be 64 lowercase hex characters")
                   .arg(spec.id);
        return true;
    }
    return false;
}

DependencySpec specFromJson(const QJsonObject& object)
{
    DependencySpec spec;
    spec.id = object.value(QStringLiteral("id")).toString();
    spec.kind = object.value(QStringLiteral("kind")).toString();
    spec.version = object.value(QStringLiteral("version")).toString();
    spec.url = object.value(QStringLiteral("url")).toString();
    spec.sha256 = object.value(QStringLiteral("sha256")).toString();
    spec.size = qint64(object.value(QStringLiteral("size")).toDouble());
    spec.file = object.value(QStringLiteral("file")).toString();
    spec.executable = object.value(QStringLiteral("executable")).toBool(false);
    const QJsonArray provides =
        object.value(QStringLiteral("provides")).toArray();
    for (const QJsonValue& value : provides)
        spec.provides.append(value.toString());
    return spec;
}

QJsonObject specToJson(const DependencySpec& spec)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), spec.id);
    object.insert(QStringLiteral("kind"), spec.kind);
    object.insert(QStringLiteral("version"), spec.version);
    if (!spec.url.isEmpty())
        object.insert(QStringLiteral("url"), spec.url);
    if (!spec.sha256.isEmpty())
        object.insert(QStringLiteral("sha256"), spec.sha256);
    if (spec.size > 0)
        object.insert(QStringLiteral("size"), double(spec.size));
    object.insert(QStringLiteral("file"), spec.file);
    if (!spec.provides.isEmpty()) {
        QJsonArray provides;
        for (const QString& role : spec.provides)
            provides.append(role);
        object.insert(QStringLiteral("provides"), provides);
    }
    if (spec.executable)
        object.insert(QStringLiteral("executable"), true);
    return object;
}

bool fileChecksum(const QString& path, QString* checksum, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot read \"%1\": %2")
                     .arg(path, file.errorString());
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        *error = QStringLiteral("cannot read \"%1\": %2")
                     .arg(path, file.errorString());
        return false;
    }
    *checksum = QString::fromLatin1(hash.result().toHex());
    return true;
}

QJsonObject emptyLock()
{
    QJsonObject lock;
    lock.insert(QStringLiteral("version"), 1);
    lock.insert(QStringLiteral("dependencies"), QJsonArray());
    return lock;
}

} // namespace

DependencyManager::DependencyManager(const QString& rootDir, QObject* parent)
    : QObject(parent)
    , m_rootDir(rootDir)
    , m_nam(new QNetworkAccessManager(this))
{
    loadLock();
}

QString DependencyManager::rootDir() const
{
    return m_rootDir;
}

QString DependencyManager::lockFilePath() const
{
    return QDir(m_rootDir).filePath(QStringLiteral("deps.lock.json"));
}

QString DependencyManager::expectedPath(const DependencySpec& spec) const
{
    return QDir(m_rootDir).filePath(
        spec.id + QLatin1Char('/') + spec.version + QLatin1Char('/')
        + spec.file);
}

void DependencyManager::loadLock()
{
    m_lock = emptyLock();
    QFile file(lockFilePath());
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcDependencies) << "cannot read" << lockFilePath() << ":"
                                  << file.errorString();
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        qCWarning(lcDependencies) << "ignoring malformed" << lockFilePath()
                                  << ":" << parseError.errorString();
        return;
    }
    QJsonObject object = document.object();
    if (!object.value(QStringLiteral("dependencies")).isArray())
        object.insert(QStringLiteral("dependencies"), QJsonArray());
    m_lock = object;
}

QJsonObject DependencyManager::lockEntry(const QString& id) const
{
    const QJsonArray entries =
        m_lock.value(QStringLiteral("dependencies")).toArray();
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("id")).toString() == id)
            return entry;
    }
    return QJsonObject();
}

bool DependencyManager::writeLock(QString* error)
{
    if (!QDir().mkpath(m_rootDir)) {
        *error = QStringLiteral("cannot create \"%1\"").arg(m_rootDir);
        return false;
    }
    QSaveFile file(lockFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QStringLiteral("cannot write \"%1\": %2")
                     .arg(lockFilePath(), file.errorString());
        return false;
    }
    file.write(QJsonDocument(m_lock).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        *error = QStringLiteral("cannot write \"%1\": %2")
                     .arg(lockFilePath(), file.errorString());
        return false;
    }
    return true;
}

bool DependencyManager::recordInLock(const DependencySpec& spec,
                                     const QString& absolutePath, bool adopted,
                                     const QString& effectiveSha256,
                                     QString* error)
{
    QJsonObject entry;
    entry.insert(QStringLiteral("id"), spec.id);
    entry.insert(QStringLiteral("version"), spec.version);
    entry.insert(QStringLiteral("path"), absolutePath);
    if (!effectiveSha256.isEmpty())
        entry.insert(QStringLiteral("sha256"), effectiveSha256);
    entry.insert(QStringLiteral("adopted"), adopted);

    QJsonArray entries = m_lock.value(QStringLiteral("dependencies")).toArray();
    bool replaced = false;
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).toObject().value(QStringLiteral("id")).toString()
            != spec.id) {
            continue;
        }
        entries.replace(i, entry);
        replaced = true;
        break;
    }
    if (!replaced)
        entries.append(entry);

    m_lock.insert(QStringLiteral("version"), 1);
    m_lock.insert(QStringLiteral("dependencies"), entries);
    return writeLock(error);
}

bool DependencyManager::loadManifest(const QString& manifestPath,
                                     QString* error)
{
    QString ignored;
    if (!error)
        error = &ignored;
    error->clear();

    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot read \"%1\": %2")
                     .arg(manifestPath, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        *error = QStringLiteral("\"%1\" is not a JSON object: %2")
                     .arg(manifestPath, parseError.errorString());
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1) {
        *error = QStringLiteral("\"%1\": unsupported manifest version (expected 1)")
                     .arg(manifestPath);
        return false;
    }
    if (!root.value(QStringLiteral("dependencies")).isArray()) {
        *error = QStringLiteral("\"%1\": \"dependencies\" must be an array")
                     .arg(manifestPath);
        return false;
    }

    QVector<DependencySpec> specs;
    QSet<QString> ids;
    const QJsonArray array =
        root.value(QStringLiteral("dependencies")).toArray();
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            *error = QStringLiteral("\"%1\": every dependency must be an object")
                         .arg(manifestPath);
            return false;
        }
        const DependencySpec spec = specFromJson(value.toObject());
        QString why;
        if (specProblem(spec, &why)) {
            *error = QStringLiteral("\"%1\": %2").arg(manifestPath, why);
            return false;
        }
        if (ids.contains(spec.id)) {
            *error = QStringLiteral("\"%1\": duplicate id \"%2\"")
                         .arg(manifestPath, spec.id);
            return false;
        }
        ids.insert(spec.id);
        specs.append(spec);
    }

    m_specs = specs;
    return true;
}

QVector<DependencySpec> DependencyManager::specs() const
{
    return m_specs;
}

DependencyStatus DependencyManager::status(const DependencySpec& spec) const
{
    DependencyStatus result;
    QString why;
    if (specProblem(spec, &why)) {
        result.error = why;
        return result;
    }

    const QJsonObject entry = lockEntry(spec.id);
    if (entry.value(QStringLiteral("version")).toString() == spec.version
        && !entry.value(QStringLiteral("path")).toString().isEmpty()) {
        result.path = entry.value(QStringLiteral("path")).toString();
    } else {
        result.path = expectedPath(spec);
    }

    const QFileInfo info(result.path);
    if (!info.exists() || !info.isFile()) {
        result.error = QStringLiteral("missing dependency '%1' (%2): \"%3\"")
                           .arg(spec.id, spec.version, result.path);
        return result;
    }
    if (spec.size > 0 && info.size() != spec.size) {
        result.error = QStringLiteral("dependency '%1' (%2): size mismatch, "
                                      "expected %3 bytes, found %4")
                           .arg(spec.id, spec.version)
                           .arg(spec.size)
                           .arg(info.size());
        return result;
    }

    QString expected = spec.sha256;
    if (expected.isEmpty())
        expected = entry.value(QStringLiteral("sha256")).toString();
    if (!expected.isEmpty()) {
        QString found;
        QString checksumError;
        if (!fileChecksum(result.path, &found, &checksumError)) {
            result.error = QStringLiteral("dependency '%1': %2")
                               .arg(spec.id, checksumError);
            return result;
        }
        if (found.compare(expected, Qt::CaseInsensitive) != 0) {
            result.error = QStringLiteral("dependency '%1' (%2): sha256 "
                                          "mismatch, expected %3, found %4")
                               .arg(spec.id, spec.version, expected, found);
            return result;
        }
    }

    result.present = true;
    return result;
}

void DependencyManager::install(
    const DependencySpec& spec,
    std::function<void(bool ok, QString error)> done)
{
    // Captured by value: the asynchronous half of `install()` outlives this
    // function, so neither the spec nor the callback may be referenced.
    const auto finish = [this, spec, done](bool ok, const QString& error) {
        emit finished(spec.id, ok, error);
        if (done)
            done(ok, error);
    };

    QString why;
    if (specProblem(spec, &why)) {
        finish(false, why);
        return;
    }
    if (spec.url.isEmpty()) {
        finish(false, QStringLiteral("dependency '%1' has no url; use adopt() "
                                     "for an existing file")
                          .arg(spec.id));
        return;
    }
    if (m_installing.contains(spec.id)) {
        finish(false, QStringLiteral("dependency '%1' is already being "
                                     "installed")
                          .arg(spec.id));
        return;
    }

    const QString destination = expectedPath(spec);
    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        finish(false, QStringLiteral("cannot create \"%1\"")
                          .arg(QFileInfo(destination).absolutePath()));
        return;
    }

    auto* output = new QSaveFile(destination, this);
    if (!output->open(QIODevice::WriteOnly)) {
        const QString error = QStringLiteral("cannot write \"%1\": %2")
                                  .arg(destination, output->errorString());
        output->deleteLater();
        finish(false, error);
        return;
    }

    m_installing.append(spec.id);
    QNetworkRequest request(QUrl(spec.url, QUrl::StrictMode));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_nam->get(request);

    auto received = std::make_shared<qint64>(0);
    auto hash = std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256);
    const auto consume = [this, reply, output, spec, received, hash]() -> bool {
        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty())
            return true;
        *received += chunk.size();
        hash->addData(chunk);
        if (output->write(chunk) != chunk.size()) {
            reply->abort();
            return false;
        }
        const qint64 total =
            spec.size > 0
                ? spec.size
                : reply->header(QNetworkRequest::ContentLengthHeader)
                      .toLongLong();
        emit progress(spec.id, *received, total);
        return true;
    };

    connect(reply, &QNetworkReply::readyRead, this, [consume] { consume(); });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, output, spec, destination, received, hash, consume,
             finish] {
                m_installing.removeAll(spec.id);
                const auto cleanup = [reply, output] {
                    reply->deleteLater();
                    output->deleteLater();
                };
                if (!consume()
                    || reply->error() != QNetworkReply::NoError) {
                    output->cancelWriting();
                    const QString error =
                        QStringLiteral("download of '%1' failed: %2")
                            .arg(spec.id, reply->errorString());
                    cleanup();
                    finish(false, error);
                    return;
                }
                if (reply->url().scheme().compare(QLatin1String("https"),
                                                  Qt::CaseInsensitive)
                    != 0) {
                    output->cancelWriting();
                    cleanup();
                    finish(false, QStringLiteral("download of '%1' was "
                                                 "redirected to a non-https URL")
                                      .arg(spec.id));
                    return;
                }
                if (spec.size > 0 && *received != spec.size) {
                    output->cancelWriting();
                    cleanup();
                    finish(false, QStringLiteral("dependency '%1': size "
                                                 "mismatch, expected %2 bytes, "
                                                 "received %3")
                                      .arg(spec.id)
                                      .arg(spec.size)
                                      .arg(*received));
                    return;
                }
                const QString found =
                    QString::fromLatin1(hash->result().toHex());
                if (found.compare(spec.sha256, Qt::CaseInsensitive) != 0) {
                    output->cancelWriting();
                    cleanup();
                    finish(false, QStringLiteral("dependency '%1': sha256 "
                                                 "mismatch, expected %2, found %3")
                                      .arg(spec.id, spec.sha256, found));
                    return;
                }
                if (!output->commit()) {
                    const QString error =
                        QStringLiteral("cannot write \"%1\": %2")
                            .arg(expectedPath(spec), output->errorString());
                    cleanup();
                    finish(false, error);
                    return;
                }
                QString lockError;
                if (!recordInLock(spec, destination, false, spec.sha256,
                                  &lockError)) {
                    cleanup();
                    finish(false, QStringLiteral("installed '%1' but could not "
                                                 "update the lockfile: %2")
                                      .arg(spec.id, lockError));
                    return;
                }
                cleanup();
                finish(true, QString());
            });
}

bool DependencyManager::adopt(const DependencySpec& spec,
                              const QString& existingPath, QString* error)
{
    QString ignored;
    if (!error)
        error = &ignored;
    error->clear();

    QString why;
    if (specProblem(spec, &why)) {
        *error = why;
        return false;
    }
    if (existingPath.isEmpty()) {
        *error = QStringLiteral("dependency '%1': existing path is empty")
                     .arg(spec.id);
        return false;
    }

    const QString absolute = QFileInfo(existingPath).absoluteFilePath();
    const QFileInfo info(absolute);
    if (!info.exists() || !info.isFile()) {
        *error = QStringLiteral("dependency '%1': \"%2\" is not a file")
                     .arg(spec.id, absolute);
        return false;
    }
    if (spec.size > 0 && info.size() != spec.size) {
        *error = QStringLiteral("dependency '%1': size mismatch, expected %2 "
                                "bytes, found %3")
                     .arg(spec.id)
                     .arg(spec.size)
                     .arg(info.size());
        return false;
    }

    QString found;
    QString checksumError;
    if (!fileChecksum(absolute, &found, &checksumError)) {
        *error = QStringLiteral("dependency '%1': %2")
                     .arg(spec.id, checksumError);
        return false;
    }
    if (!spec.sha256.isEmpty()
        && found.compare(spec.sha256, Qt::CaseInsensitive) != 0) {
        *error = QStringLiteral("dependency '%1': sha256 mismatch, expected "
                                "%2, found %3")
                     .arg(spec.id, spec.sha256, found);
        return false;
    }

    return recordInLock(spec, absolute, true,
                        spec.sha256.isEmpty() ? found : spec.sha256, error);
}

QJsonObject DependencyManager::lock() const
{
    return m_lock;
}

} // namespace Daqster
