#include "Scene/SceneVariables.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcSceneVariables, "daqster.scene.variables")

namespace Daqster {
namespace {

/// Group of the document that holds the token table (`paths/DAQSTER_DEPS`).
const char* const kPathsGroup = "paths";

/// One value of the table, however it was found.
struct Lookup
{
    bool    isSet = false;
    QString value;
};

/// `mtime:size` of the document — the cache's proof that it is still current.
/// A missing file is a valid state (first run), so it gets its own stamp.
QString fileStamp(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return QStringLiteral("-");

    return QStringLiteral("%1:%2")
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(info.size());
}

/// Read @p path as a JSON object.
///
/// A **missing** file is not an error (first run) — @p ok stays true and the
/// document is empty. A file that exists but cannot be read or parsed is an
/// error: writing over it would silently drop every other key it carries.
QJsonObject readDocument(const QString& path, bool* ok)
{
    if (ok != nullptr)
        *ok = true;

    const QFileInfo info(path);
    if (!info.exists())
        return QJsonObject();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(lcSceneVariables).noquote()
            << QStringLiteral("SceneVariables: cannot read %1: %2")
                   .arg(path, file.errorString());
        if (ok != nullptr)
            *ok = false;
        return QJsonObject();
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qCWarning(lcSceneVariables).noquote()
            << QStringLiteral("SceneVariables: %1 is not a JSON object: %2")
                   .arg(path, parseError.errorString());
        if (ok != nullptr)
            *ok = false;
        return QJsonObject();
    }

    return document.object();
}

/// Write @p document atomically; missing directories are created.
bool writeDocument(const QString& path, const QJsonObject& document, QString* error)
{
    const auto fail = [error](const QString& reason) {
        if (error != nullptr)
            *error = reason;
        return false;
    };

    if (path.isEmpty())
        return fail(QStringLiteral("the scene variable document has no path"));

    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return fail(QStringLiteral("cannot create %1")
                        .arg(QFileInfo(path).absolutePath()));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));

    const QByteArray bytes = QJsonDocument(document).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        const QString reason = file.errorString();
        file.cancelWriting();
        return fail(QStringLiteral("cannot write %1: %2").arg(path, reason));
    }

    if (!file.commit())
        return fail(QStringLiteral("cannot write %1: %2").arg(path, file.errorString()));

    return true;
}

/// The `paths` object of @p document as a table. Empty values are dropped —
/// they never resolve anything (see the header) and only shadow the next level.
QMap<QString, QString> tokensFromDocument(const QJsonObject& document)
{
    QMap<QString, QString> tokens;

    const QJsonObject paths = document.value(QLatin1String(kPathsGroup)).toObject();
    for (auto it = paths.constBegin(); it != paths.constEnd(); ++it) {
        if (!it.value().isString())
            continue;

        const QString value = it.value().toString().trimmed();
        if (!value.isEmpty())
            tokens.insert(it.key(), value);
    }

    return tokens;
}

/// Why @p name cannot be a token name, or an empty string when it can.
///
/// The name is written into `.flow` files, so anything that looks like syntax
/// (`$`, braces, `:`) or like a path (`/`, `\`) would make the expression
/// ambiguous the next time it is expanded.
QString nameProblem(const QString& name)
{
    if (name.isEmpty())
        return QStringLiteral("the token name is empty");

    if (name != name.trimmed())
        return QStringLiteral("\"%1\" has surrounding whitespace").arg(name);

    const QString forbidden = QStringLiteral("${}:/\\");
    for (const QChar character : name) {
        if (forbidden.contains(character))
            return QStringLiteral("\"%1\" contains '%2'").arg(name, QString(character));
    }

    return QString();
}

/// Resolve @p name: overrides → machine config → environment → not set.
Lookup lookup(const QString& name, const SceneVariables::Overrides& overrides,
              const QMap<QString, QString>& table)
{
    const auto overridden = overrides.constFind(name);
    if (overridden != overrides.constEnd() && !overridden.value().isEmpty())
        return Lookup{ true, overridden.value() };

    const auto configured = table.constFind(name);
    if (configured != table.constEnd() && !configured.value().isEmpty())
        return Lookup{ true, configured.value() };

    const QByteArray key = name.toUtf8();
    if (qEnvironmentVariableIsSet(key.constData())) {
        const QString value = qEnvironmentVariable(key.constData());
        if (!value.isEmpty())
            return Lookup{ true, value };
    }

    return Lookup{};
}

} // namespace

SceneVariables::SceneVariables()
    : m_filePath(defaultFilePath())
{
}

SceneVariables& SceneVariables::instance()
{
    static SceneVariables variables;
    return variables;
}

QString SceneVariables::defaultFilePath()
{
    // Tests and unusual setups: one file, chosen explicitly.
    const QByteArray overridePath = qgetenv("DAQSTER_SCENE_VARIABLES");
    if (!overridePath.isEmpty())
        return QFile::decodeName(overridePath);

    const QString fileName = QStringLiteral("scene-variables.json");
    const QString genericRoot =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);

    QStringList candidates;
#ifdef Q_OS_WIN
    // No launcher-independent config root exists on Windows, so there is one
    // candidate and the IDE/NodeRunner split cannot happen there anyway.
    candidates << QDir(genericRoot).filePath(QStringLiteral("Daqster/scene-variables.json"));
#else
    // Launcher-independent first: `ApplicationsManager` starts child apps with
    // `XDG_CONFIG_HOME=~/.config/daqster`, so `GenericConfigLocation` alone
    // would give the IDE and `NodeRunner` two tables for one machine.
    candidates << QDir::homePath() + QStringLiteral("/.config/Daqster/") + fileName;
    if (!genericRoot.isEmpty())
        candidates << QDir(genericRoot).filePath(QStringLiteral("Daqster/scene-variables.json"));
#endif

    // First candidate that already holds a table; otherwise the first one
    // (creating it there is the normal first run).
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }

    return candidates.first();
}

QStringList SceneVariables::knownTokenNames()
{
    return QStringList{
        QStringLiteral("DAQSTER_DEPS"),        // binaries/, models/, tts_voice/
        QStringLiteral("DAQSTER_LLAMA"),       // llama.cpp binaries + GGUF models
        QStringLiteral("DAQSTER_VIDEO_FILE"),  // the clip a scene was written against
    };
}

QString SceneVariables::filePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_filePath;
}

void SceneVariables::setFilePath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = path.isEmpty() ? defaultFilePath() : path;
    m_tokens.clear();
    m_stamp.clear();
    m_loaded = false;
}

void SceneVariables::ensureLoaded() const
{
    // Caller holds m_mutex (or is the constructor-time path).
    if (!m_loaded || m_stamp != fileStamp(m_filePath)) {
        bool ok = true;
        const QJsonObject document = readDocument(m_filePath, &ok);
        // A broken document caches as empty; the stamp still records it, so the
        // parse error is logged once per change, not once per lookup.
        m_tokens = ok ? tokensFromDocument(document) : QMap<QString, QString>();
        m_stamp = fileStamp(m_filePath);
        m_loaded = true;
    }
}

SceneVariableExpansion SceneVariables::expandDetailed(const QString& raw,
                                                      const Overrides& overrides) const
{
    SceneVariableExpansion result;
    result.text = raw;

    // Fast path: an ordinary path carries no '$' and is returned untouched.
    if (!raw.contains(QLatin1Char('$')))
        return result;

    QMap<QString, QString> table;
    {
        QMutexLocker locker(&m_mutex);
        ensureLoaded();
        table = m_tokens;
    }

    const auto report = [&result](const QString& name, const QString& message) {
        if (!result.token.isEmpty())
            return;   // the first broken expression is the one worth naming

        result.name = name;
        result.token = QStringLiteral("${") + name + QLatin1Char('}');
        result.message = message;
    };

    QString out;
    out.reserve(raw.size());

    const int size = raw.size();
    for (int i = 0; i < size;) {
        if (raw.at(i) != QLatin1Char('$')) {
            out.append(raw.at(i));
            ++i;
            continue;
        }

        // `$$` → `$`: the only way to write a literal dollar sign.
        if (i + 1 < size && raw.at(i + 1) == QLatin1Char('$')) {
            out.append(QLatin1Char('$'));
            i += 2;
            continue;
        }

        // `${...}`; without a closing brace the string is left as it is.
        if (i + 1 < size && raw.at(i + 1) == QLatin1Char('{')) {
            const int close = raw.indexOf(QLatin1Char('}'), i + 2);
            if (close > i) {
                const QString body = raw.mid(i + 2, close - i - 2);
                const int defaultSep = body.indexOf(QStringLiteral(":-"));
                const int requiredSep = body.indexOf(QStringLiteral(":?"));
                // `${X:?msg}` carries no fallback: either the value or a report.
                // Checked before `:-` — two separators in one expression are
                // not a valid case, but the first one wins.
                const bool required = requiredSep >= 0
                    && (defaultSep < 0 || requiredSep < defaultSep);
                const int separator = required ? -1 : defaultSep;
                const QString name = (separator >= 0     ? body.left(separator)
                                      : required         ? body.left(requiredSep)
                                                         : body)
                                         .trimmed();
                const QString fallback =
                    separator >= 0 ? body.mid(separator + 2) : QString();

                if (name.isEmpty()) {
                    // `${}` is not a token; keep it visible, report nothing.
                    out.append(raw.mid(i, close - i + 1));
                    i = close + 1;
                    continue;
                }

                const Lookup found = lookup(name, overrides, table);
                if (found.isSet) {
                    out.append(found.value);
                } else if (separator >= 0) {
                    out.append(fallback);   // `${X:-default}` with X unset/empty
                } else if (required) {
                    // `${X:?msg}`: no value, no literal — the caller turns the
                    // reported token into a visible node error.
                    report(name, body.mid(requiredSep + 2).trimmed());
                } else {
                    report(name, QString());
                    out.append(raw.mid(i, close - i + 1));   // stays visible
                }

                i = close + 1;
                continue;
            }
        }

        out.append(raw.at(i));
        ++i;
    }

    result.text = out;
    return result;
}

QString SceneVariables::expand(const QString& raw, QString* unexpandedToken) const
{
    const SceneVariableExpansion expansion = expandDetailed(raw);
    if (unexpandedToken != nullptr)
        *unexpandedToken = expansion.token;
    return expansion.text;
}

QString SceneVariables::sceneDirectory() const
{
    QMutexLocker locker(&m_mutex);
    return m_sceneDirectory;
}

void SceneVariables::setSceneDirectory(const QString& directory)
{
    QMutexLocker locker(&m_mutex);
    m_sceneDirectory = directory.isEmpty() ? QString() : QDir::cleanPath(directory);
}

void SceneVariables::adoptSceneFile(const QString& scenePath)
{
    // An empty path is not "the scene lives in the cwd": it is a scene that did
    // not come from a file (a REST body, a reload of the running graph), and it
    // keeps the directory it was loaded from.
    if (scenePath.isEmpty())
        return;

    setSceneDirectory(QFileInfo(scenePath).absolutePath());
}

QString SceneVariables::resolvePath(const QString& raw, QString* unexpandedToken,
                                    QString* reason) const
{
    if (unexpandedToken != nullptr)
        unexpandedToken->clear();
    if (reason != nullptr)
        reason->clear();

    const SceneVariableExpansion expansion = expandDetailed(raw);
    if (unexpandedToken != nullptr)
        *unexpandedToken = expansion.token;

    // An unresolved `${X}` is not a path yet: hand back the visible literal so
    // save() writes the same token it read, and let the caller name the token.
    if (!expansion.isComplete())
        return expansion.text.trimmed();

    const QString path = expansion.text.trimmed();
    if (path.isEmpty())
        return QString();   // nothing configured — a valid state, not a failure

    if (QDir::isAbsolutePath(path))
        return path;        // a path that is already whole needs no anchor

    const QString directory = sceneDirectory();
    if (directory.isEmpty()) {
        if (reason != nullptr) {
            *reason = QStringLiteral("\"%1\" is relative and the scene directory is "
                                     "not known — the scene was not loaded from a file")
                          .arg(path);
        }
        return QString();
    }

    // Lexical only: the file may not exist yet and the scene may live behind a
    // symlink, so nothing here asks the filesystem what ".." means.
    return QDir::cleanPath(QDir(directory).filePath(path));
}

QString SceneVariables::pathToken(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    ensureLoaded();
    return m_tokens.value(name);
}

QMap<QString, QString> SceneVariables::pathTokens() const
{
    QMutexLocker locker(&m_mutex);
    ensureLoaded();
    return m_tokens;
}

bool SceneVariables::setPathToken(const QString& name, const QString& value, QString* error)
{
    const QString problem = nameProblem(name);
    if (!problem.isEmpty()) {
        if (error != nullptr)
            *error = problem;
        return false;
    }

    if (value.trimmed().isEmpty())
        return removePathToken(name);

    QMutexLocker locker(&m_mutex);

    bool readable = true;
    QJsonObject document = readDocument(m_filePath, &readable);
    if (!readable) {
        // Refuse to overwrite a document we could not read: every other key in
        // it would be lost.
        if (error != nullptr)
            *error = QStringLiteral("%1 is not readable JSON").arg(m_filePath);
        return false;
    }

    QJsonObject paths = document.value(QLatin1String(kPathsGroup)).toObject();
    paths.insert(name, value.trimmed());
    document.insert(QLatin1String(kPathsGroup), paths);

    if (!writeDocument(m_filePath, document, error))
        return false;

    m_tokens = tokensFromDocument(document);
    m_stamp = fileStamp(m_filePath);
    m_loaded = true;
    return true;
}

bool SceneVariables::removePathToken(const QString& name)
{
    QMutexLocker locker(&m_mutex);

    bool readable = true;
    QJsonObject document = readDocument(m_filePath, &readable);
    if (!readable)
        return false;

    QJsonObject paths = document.value(QLatin1String(kPathsGroup)).toObject();
    if (!paths.contains(name))
        return true;   // nothing to remove

    paths.remove(name);
    document.insert(QLatin1String(kPathsGroup), paths);

    if (!writeDocument(m_filePath, document, nullptr))
        return false;

    m_tokens = tokensFromDocument(document);
    m_stamp = fileStamp(m_filePath);
    m_loaded = true;
    return true;
}

int SceneVariables::seedFromEnvironment(const QStringList& names)
{
    const QStringList considered = names.isEmpty() ? knownTokenNames() : names;

    QMutexLocker locker(&m_mutex);

    bool readable = true;
    QJsonObject document = readDocument(m_filePath, &readable);
    if (!readable)
        return 0;

    const QMap<QString, QString> existing = tokensFromDocument(document);
    QJsonObject paths = document.value(QLatin1String(kPathsGroup)).toObject();

    int written = 0;
    for (const QString& name : considered) {
        if (name.isEmpty() || existing.contains(name))
            continue;   // an existing table is never overwritten

        const QByteArray key = name.toUtf8();
        if (!qEnvironmentVariableIsSet(key.constData()))
            continue;

        const QString value = qEnvironmentVariable(key.constData()).trimmed();
        if (value.isEmpty())
            continue;   // copying "" would only shadow the config

        paths.insert(name, value);
        ++written;
    }

    if (written == 0)
        return 0;

    document.insert(QLatin1String(kPathsGroup), paths);
    if (!writeDocument(m_filePath, document, nullptr))
        return 0;

    m_tokens = tokensFromDocument(document);
    m_stamp = fileStamp(m_filePath);
    m_loaded = true;
    return written;
}

} // namespace Daqster
