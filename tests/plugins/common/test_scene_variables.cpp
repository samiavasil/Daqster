#include "test_scene_variables.h"

#include <Scene/SceneVariables.h>
#include <Scene/SceneLayout.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>

using Daqster::SceneVariableExpansion;
using Daqster::SceneVariables;

// ═══════════════════════════════════════════════════════════════════════
//  Isolation: temporary XDG_CONFIG_HOME + HOME, no inherited tokens
// ═══════════════════════════════════════════════════════════════════════

void TestSceneVariables::init()
{
    m_environment.clear();

    m_configDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_configDir->isValid());
    m_homeDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_homeDir->isValid());

    // The resolver has two candidates: `<HOME>/.config/Daqster/…` (launcher
    // independent) and `<GenericConfigLocation>/Daqster/…`. Both are moved into
    // a temp directory, so the real machine table is never read and never
    // written — the same guarantee `ScopedDataHome` gives the server tests.
    overrideEnvironment("XDG_CONFIG_HOME", QFile::encodeName(m_configDir->path()));
    overrideEnvironment("HOME", QFile::encodeName(m_homeDir->path()));

    unsetEnvironment("DAQSTER_SCENE_VARIABLES");
    unsetEnvironment("DAQSTER_DEPS");
    unsetEnvironment("DAQSTER_LLAMA");
    unsetEnvironment("DAQSTER_LLAMA_MODELS");
    unsetEnvironment("DAQSTER_VIDEO_FILE");

    m_document = m_configDir->path() + QStringLiteral("/Daqster/scene-variables.json");
    SceneVariables::instance().setFilePath(m_document);
    // Ambient state survives between tests (one process, one instance): start
    // every case with no scene adopted, exactly like a fresh process.
    SceneVariables::instance().setSceneDirectory(QString());
}

void TestSceneVariables::cleanup()
{
    SceneVariables::instance().setSceneDirectory(QString());
    // Back to the default path *before* the temp roots disappear, then restore
    // the environment the test process was started with.
    SceneVariables::instance().setFilePath(QString());
    restoreEnvironment();

    m_configDir.reset();
    m_homeDir.reset();
}

void TestSceneVariables::overrideEnvironment(const QByteArray& name, const QByteArray& value)
{
    const bool known = std::any_of(
        m_environment.cbegin(), m_environment.cend(),
        [&name](const EnvSnapshot& snapshot) { return snapshot.name == name; });

    if (!known) {
        m_environment.append(EnvSnapshot{ name,
                                          qEnvironmentVariableIsSet(name.constData()),
                                          qgetenv(name.constData()) });
    }

    qputenv(name, value);
}

void TestSceneVariables::unsetEnvironment(const QByteArray& name)
{
    const bool known = std::any_of(
        m_environment.cbegin(), m_environment.cend(),
        [&name](const EnvSnapshot& snapshot) { return snapshot.name == name; });

    if (!known) {
        m_environment.append(EnvSnapshot{ name,
                                          qEnvironmentVariableIsSet(name.constData()),
                                          qgetenv(name.constData()) });
    }

    qunsetenv(name.constData());
}

void TestSceneVariables::restoreEnvironment()
{
    for (const EnvSnapshot& snapshot : m_environment) {
        if (snapshot.wasSet)
            qputenv(snapshot.name, snapshot.value);
        else
            qunsetenv(snapshot.name.constData());
    }
    m_environment.clear();
}

// ═══════════════════════════════════════════════════════════════════════
//  Syntax
// ═══════════════════════════════════════════════════════════════════════

void TestSceneVariables::expandsTheFourSyntaxes()
{
    SceneVariables& variables = SceneVariables::instance();
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_DEPS"), QStringLiteral("/deps")));
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_LLAMA"), QStringLiteral("/llama")));

    // `${X}` with a value.
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_DEPS}/ggml-small.bin")),
             QStringLiteral("/deps/ggml-small.bin"));

    // `${X:-default}`: the default when X is missing, the value otherwise.
    QCOMPARE(variables.expand(QStringLiteral("${MISSING:-/fallback}/x.bin")),
             QStringLiteral("/fallback/x.bin"));
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_LLAMA:-/fallback}/cli")),
             QStringLiteral("/llama/cli"));

    // `${X:?message}` with a value — the message is irrelevant.
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_LLAMA:?set DAQSTER_LLAMA}")),
             QStringLiteral("/llama"));

    // An unknown `${X}` without a default stays **visible** and is reported.
    QString token;
    QCOMPARE(variables.expand(QStringLiteral("${NOPE}/x.bin"), &token),
             QStringLiteral("${NOPE}/x.bin"));
    QCOMPARE(token, QStringLiteral("${NOPE}"));

    // No `$` at all: the fast path returns the string untouched.
    QCOMPARE(variables.expand(QStringLiteral("plain/path.bin")),
             QStringLiteral("plain/path.bin"));
}

void TestSceneVariables::precedenceIsOverrideConfigEnvironment()
{
    SceneVariables& variables = SceneVariables::instance();
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_DEPS"), QStringLiteral("/from-config")));
    overrideEnvironment("DAQSTER_DEPS", "/from-env");

    SceneVariables::Overrides overrides;
    overrides.insert(QStringLiteral("DAQSTER_DEPS"), QStringLiteral("/from-override"));
    QCOMPARE(variables.expandDetailed(QStringLiteral("${DAQSTER_DEPS}/x"), overrides).text,
             QStringLiteral("/from-override/x"));

    // No override: the persisted table beats the environment.
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_DEPS}/x")),
             QStringLiteral("/from-config/x"));

    // No table entry: the environment is next.
    QVERIFY(variables.removePathToken(QStringLiteral("DAQSTER_DEPS")));
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_DEPS}/x")),
             QStringLiteral("/from-env/x"));

    // Nothing set anywhere: the literal stays.
    unsetEnvironment("DAQSTER_DEPS");
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_DEPS}/x")),
             QStringLiteral("${DAQSTER_DEPS}/x"));

    // An *empty* override is not a value — it must not shadow the table (an
    // empty token would turn `${DEPS}/x` into `/x`, a path that exists only in
    // theory).
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_LLAMA"), QStringLiteral("/llama-config")));
    SceneVariables::Overrides emptyOverride;
    emptyOverride.insert(QStringLiteral("DAQSTER_LLAMA"), QString());
    QCOMPARE(variables.expandDetailed(QStringLiteral("${DAQSTER_LLAMA}"), emptyOverride).text,
             QStringLiteral("/llama-config"));
}

void TestSceneVariables::requiredTokenReportsTokenAndMessage()
{
    SceneVariables& variables = SceneVariables::instance();

    const SceneVariableExpansion missing =
        variables.expandDetailed(QStringLiteral("${DAQSTER_DEPS:?set DAQSTER_DEPS}/x.bin"));
    // The expression itself becomes an empty string ("no value, no literal") —
    // `/x.bin` is what a caller would try to open if it ignored the report, so
    // the report is the part that matters.
    QCOMPARE(missing.text, QStringLiteral("/x.bin"));
    QVERIFY(!missing.isComplete());
    QCOMPARE(missing.token, QStringLiteral("${DAQSTER_DEPS}"));
    QCOMPARE(missing.name, QStringLiteral("DAQSTER_DEPS"));
    QCOMPARE(missing.message, QStringLiteral("set DAQSTER_DEPS"));

    // Once the token resolves, the same expression is complete and silent.
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_DEPS"), QStringLiteral("/deps")));
    const SceneVariableExpansion complete =
        variables.expandDetailed(QStringLiteral("${DAQSTER_DEPS:?set DAQSTER_DEPS}/x.bin"));
    QCOMPARE(complete.text, QStringLiteral("/deps/x.bin"));
    QVERIFY(complete.isComplete());
    QVERIFY(complete.message.isEmpty());
}

void TestSceneVariables::dollarEscapeAndMalformedExpressions()
{
    SceneVariables& variables = SceneVariables::instance();

    QCOMPARE(variables.expand(QStringLiteral("$$HOME")), QStringLiteral("$HOME"));
    QCOMPARE(variables.expand(QStringLiteral("price$$5")), QStringLiteral("price$5"));

    // `${}` is not a token: it stays as written and reports nothing.
    const SceneVariableExpansion empty = variables.expandDetailed(QStringLiteral("${}"));
    QCOMPARE(empty.text, QStringLiteral("${}"));
    QVERIFY(empty.isComplete());

    // An unclosed `${` is not an expression either.
    QCOMPARE(variables.expand(QStringLiteral("${UNCLOSED/x")),
             QStringLiteral("${UNCLOSED/x"));
}

void TestSceneVariables::expandWrapperReportsToken()
{
    SceneVariables& variables = SceneVariables::instance();

    QString token = QStringLiteral("sentinel");
    QCOMPARE(variables.expand(QStringLiteral("${MISSING}/y"), &token),
             QStringLiteral("${MISSING}/y"));
    QCOMPARE(token, QStringLiteral("${MISSING}"));

    // The out-param is cleared when everything resolves.
    QVERIFY(variables.setPathToken(QStringLiteral("MISSING"), QStringLiteral("/here")));
    token = QStringLiteral("sentinel");
    QCOMPARE(variables.expand(QStringLiteral("${MISSING}/y"), &token),
             QStringLiteral("/here/y"));
    QVERIFY(token.isEmpty());
}

// ═══════════════════════════════════════════════════════════════════════
//  Persistence
// ═══════════════════════════════════════════════════════════════════════

void TestSceneVariables::tokensRoundTripThroughTheDocument()
{
    SceneVariables& variables = SceneVariables::instance();

    // A document with an unrelated key: writing a token must not destroy it.
    QVERIFY(QDir().mkpath(QFileInfo(m_document).absolutePath()));
    {
        QFile seed(m_document);
        QVERIFY(seed.open(QIODevice::WriteOnly));
        seed.write(R"({"version": 1, "paths": {}})");   // NOLINT
    }
    variables.setFilePath(m_document);

    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_DEPS"), QStringLiteral("/deps")));
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_LLAMA"), QStringLiteral("/llama")));

    QFile file(m_document);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject document = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    QCOMPARE(document.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(document.value(QStringLiteral("paths")).toObject()
                 .value(QStringLiteral("DAQSTER_DEPS")).toString(),
             QStringLiteral("/deps"));

    // A cache reset re-reads the file: the table is not memory-only.
    variables.setFilePath(m_document);
    QCOMPARE(variables.pathToken(QStringLiteral("DAQSTER_DEPS")), QStringLiteral("/deps"));
    QCOMPARE(variables.pathTokens().value(QStringLiteral("DAQSTER_LLAMA")),
             QStringLiteral("/llama"));

    // An empty value removes the key (same effect as removePathToken()).
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_LLAMA"), QString()));
    QVERIFY(!variables.pathTokens().contains(QStringLiteral("DAQSTER_LLAMA")));
}

void TestSceneVariables::rejectsNamesThatLookLikeSyntax()
{
    SceneVariables& variables = SceneVariables::instance();

    const QStringList rejected{
        QString(),                 // empty
        QStringLiteral("${X}"),    // braces and '$' belong to the syntax
        QStringLiteral("A:B"),     // the default/required separator
        QStringLiteral("A/B"),     // a path, not a name
        QStringLiteral(" A"),      // surrounding whitespace
    };

    for (const QString& name : rejected) {
        QString error;
        QVERIFY2(!variables.setPathToken(name, QStringLiteral("/x"), &error),
                 qPrintable(QStringLiteral("accepted \"%1\"").arg(name)));
        QVERIFY(!error.isEmpty());
    }

    QVERIFY(variables.pathTokens().isEmpty());
}

void TestSceneVariables::seedFromEnvironmentKeepsExistingValue()
{
    SceneVariables& variables = SceneVariables::instance();
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_DEPS"), QStringLiteral("/configured")));
    overrideEnvironment("DAQSTER_DEPS", "/from-env");
    overrideEnvironment("DAQSTER_LLAMA", "/llama-env");

    // Only the *absent* key is copied: an existing configuration must not drift
    // back to whatever the launcher happened to export.
    QCOMPARE(variables.seedFromEnvironment(QStringList{
                 QStringLiteral("DAQSTER_DEPS"),
                 QStringLiteral("DAQSTER_LLAMA"),
                 QStringLiteral("DAQSTER_VIDEO_FILE") }),
             1);
    QCOMPARE(variables.pathToken(QStringLiteral("DAQSTER_DEPS")), QStringLiteral("/configured"));
    QCOMPARE(variables.pathToken(QStringLiteral("DAQSTER_LLAMA")), QStringLiteral("/llama-env"));
    QVERIFY(!variables.pathTokens().contains(QStringLiteral("DAQSTER_VIDEO_FILE")));

    // Nothing left to copy on a second run.
    QCOMPARE(variables.seedFromEnvironment(QStringList{ QStringLiteral("DAQSTER_DEPS"),
                                                        QStringLiteral("DAQSTER_LLAMA") }),
             0);
}

void TestSceneVariables::derivedTokensFollowTheLayout()
{
    SceneVariables& variables = SceneVariables::instance();

    // The machine configures **one** root; the two llama.cpp names are joins.
    overrideEnvironment("DAQSTER_DEPS", "/opt/deps");
    const auto under = [](const QString& relative) {
        return QDir(QStringLiteral("/opt/deps")).filePath(relative);
    };

    QCOMPARE(QDir::fromNativeSeparators(variables.expand(QStringLiteral("${DAQSTER_LLAMA}"))),
             QStringLiteral("/opt/deps/llama.cpp/build"));
    QCOMPARE(QDir::fromNativeSeparators(
                 variables.expand(QStringLiteral("${DAQSTER_LLAMA}/bin/llama-server"))),
             QStringLiteral("/opt/deps/llama.cpp/build/bin/llama-server"));
    QCOMPARE(QDir::fromNativeSeparators(variables.expand(QStringLiteral("${DAQSTER_LLAMA_MODELS}"))),
             QStringLiteral("/opt/deps/models"));

    // tokenValue() is the same precedence minus overrides — the settings UI and
    // SceneLayout ask this, so it must not answer differently from expand().
    QCOMPARE(variables.tokenValue(QStringLiteral("DAQSTER_LLAMA")),
             variables.expand(QStringLiteral("${DAQSTER_LLAMA}")));

    QString reason;
    const Daqster::SceneLayout layout = Daqster::SceneLayout::fromMachine(&reason);
    QVERIFY2(reason.isEmpty(), qPrintable(reason));
    QCOMPARE(layout.depsRoot(), QStringLiteral("/opt/deps"));
    QCOMPARE(layout.llamaBuildRoot(), under(QStringLiteral("llama.cpp/build")));
    QCOMPARE(layout.llamaBinDir(), under(QStringLiteral("llama.cpp/build/bin")));
    QCOMPARE(layout.modelsDir(), under(QStringLiteral("models")));
    QCOMPARE(layout.modelsDirFor(QStringLiteral("llm")), under(QStringLiteral("models/llm")));
    QCOMPARE(layout.piperBin(), under(QStringLiteral("piper/piper")));
    QCOMPARE(layout.piperVoicesDir(), under(QStringLiteral("piper-voices")));
    QCOMPARE(layout.whisperBin(), under(QStringLiteral("whisper.cpp/build/bin/whisper-server")));
    QCOMPARE(layout.sdServer(), under(QStringLiteral("sd-server")));

    // An unknown kind is not a directory somebody invented: it is empty.
    QCOMPARE(layout.modelsDirFor(QStringLiteral("nope")), QString());
    QCOMPARE(layout.modelsDirFor(QString()), QString());
}

void TestSceneVariables::explicitLlamaValueWinsOverTheLayout()
{
    SceneVariables& variables = SceneVariables::instance();
    overrideEnvironment("DAQSTER_DEPS", "/opt/deps");

    // A table value written before the layout existed keeps winning.
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_LLAMA"),
                                   QStringLiteral("/my/llama/build")));
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_LLAMA}")),
             QStringLiteral("/my/llama/build"));

    // The environment is the same: an exported name is an explicit decision.
    overrideEnvironment("DAQSTER_LLAMA_MODELS", "/my/models");
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_LLAMA_MODELS}")),
             QStringLiteral("/my/models"));

    // Caller overrides beat both, including a root override that the derived
    // value has to follow (the join must use the overridden root).
    SceneVariables::Overrides overrides;
    overrides.insert(QStringLiteral("DAQSTER_LLAMA"), QStringLiteral("/from/override"));
    QCOMPARE(variables.expandDetailed(QStringLiteral("${DAQSTER_LLAMA}"), overrides).text,
             QStringLiteral("/from/override"));

    QVERIFY(variables.removePathToken(QStringLiteral("DAQSTER_LLAMA")));
    unsetEnvironment("DAQSTER_LLAMA_MODELS");
    SceneVariables::Overrides rootOverride;
    rootOverride.insert(QStringLiteral("DAQSTER_DEPS"), QStringLiteral("/other/deps"));
    QCOMPARE(QDir::fromNativeSeparators(
                 variables.expandDetailed(QStringLiteral("${DAQSTER_LLAMA}"), rootOverride).text),
             QStringLiteral("/other/deps/llama.cpp/build"));
}

void TestSceneVariables::missingDepsRootIsEmptyWithAReason()
{
    // init() unsets `DAQSTER_DEPS` and the temp table is empty, so this is the
    // "machine configured nothing" case.
    QString reason;
    const Daqster::SceneLayout layout = Daqster::SceneLayout::fromMachine(&reason);
    QVERIFY(!layout.isValid());
    QVERIFY2(!reason.isEmpty(), "an empty root must say why, not answer '/'");
    QVERIFY(layout.llamaBinDir().isEmpty());
    QVERIFY(layout.modelsDirFor(QStringLiteral("llm")).isEmpty());
    QVERIFY(layout.piperBin().isEmpty());
    QVERIFY(layout.whisperBin().isEmpty());

    // The tokens stay visible in the scene instead of becoming "/llama.cpp/build".
    SceneVariables& variables = SceneVariables::instance();
    QString token;
    QCOMPARE(variables.expand(QStringLiteral("${DAQSTER_LLAMA}/bin/x"), &token),
             QStringLiteral("${DAQSTER_LLAMA}/bin/x"));
    QCOMPARE(token, QStringLiteral("${DAQSTER_LLAMA}"));
    QVERIFY(variables.tokenValue(QStringLiteral("DAQSTER_LLAMA")).isEmpty());
}

void TestSceneVariables::defaultPathIsOneRootPerMachine()
{
    const QString path = SceneVariables::defaultFilePath();
    QVERIFY2(path.endsWith(QStringLiteral("/Daqster/scene-variables.json")), qPrintable(path));
    QVERIFY2(path.startsWith(m_configDir->path()) || path.startsWith(m_homeDir->path()),
             qPrintable(path));

    // The explicit override wins — this is what keeps a test off the real file.
    const QString explicitPath = m_homeDir->path() + QStringLiteral("/explicit.json");
    overrideEnvironment("DAQSTER_SCENE_VARIABLES", QFile::encodeName(explicitPath));
    QCOMPARE(SceneVariables::defaultFilePath(), explicitPath);
}

// ═══════════════════════════════════════════════════════════════════════
//  Relative paths — the second half of "a scene is portable"
// ═══════════════════════════════════════════════════════════════════════

void TestSceneVariables::resolvePathAnchorsRelativePathsToTheScene()
{
    SceneVariables& variables = SceneVariables::instance();
    QTemporaryDir sceneDir;
    QVERIFY(sceneDir.isValid());

    QString token;
    QString reason;

    // No scene adopted: an absolute path needs no anchor and is returned as is,
    // including one whose file does not exist (existence is the caller's call).
    QCOMPARE(variables.resolvePath(QStringLiteral("/deps/clip.mp4"), &token, &reason),
             QStringLiteral("/deps/clip.mp4"));
    QVERIFY(token.isEmpty());
    QVERIFY(reason.isEmpty());

    // Tokens still expand exactly as before — the anchor is only for the result.
    QVERIFY(variables.setPathToken(QStringLiteral("DAQSTER_DEPS"), QStringLiteral("/deps")));
    QCOMPARE(variables.resolvePath(QStringLiteral("${DAQSTER_DEPS}/clip.mp4")),
             QStringLiteral("/deps/clip.mp4"));

    // A scene is adopted through the one helper the hosts call: it takes the
    // *file* path and keeps its directory.
    const QString sceneFile = sceneDir.path() + QStringLiteral("/video_e2e.flow");
    variables.adoptSceneFile(sceneFile);
    QCOMPARE(variables.sceneDirectory(), QDir::cleanPath(sceneDir.path()));

    // Relative → scene directory, lexically normalised (no filesystem lookup:
    // the file need not exist and the scene may live behind a symlink).
    QCOMPARE(variables.resolvePath(QStringLiteral("bars_h265_1080p25_x3.mp4")),
             QDir::cleanPath(sceneDir.path() + QStringLiteral("/bars_h265_1080p25_x3.mp4")));
    QCOMPARE(variables.resolvePath(QStringLiteral("./media/../clip.mp4")),
             QDir::cleanPath(sceneDir.path() + QStringLiteral("/clip.mp4")));

    // Idempotent: resolving an already resolved path changes nothing.
    QCOMPARE(variables.resolvePath(variables.resolvePath(QStringLiteral("clip.mp4"))),
             QDir::cleanPath(sceneDir.path() + QStringLiteral("/clip.mp4")));

    // A token that resolves to a relative value is anchored as well.
    QVERIFY(variables.setPathToken(QStringLiteral("RELATIVE_ROOT"), QStringLiteral("media")));
    QCOMPARE(variables.resolvePath(QStringLiteral("${RELATIVE_ROOT}/clip.mp4")),
             QDir::cleanPath(sceneDir.path() + QStringLiteral("/media/clip.mp4")));

    // An in-memory scene has no path of its own: it keeps the directory it was
    // loaded from rather than falling back to the cwd.
    variables.adoptSceneFile(QString());
    QCOMPARE(variables.sceneDirectory(), QDir::cleanPath(sceneDir.path()));

    // An empty field is "no file chosen", not a failure: empty, no reason.
    reason = QStringLiteral("sentinel");
    QVERIFY(variables.resolvePath(QString(), &token, &reason).isEmpty());
    QVERIFY(reason.isEmpty());

    // Clearing the ambient state is explicit.
    variables.setSceneDirectory(QString());
    QVERIFY(variables.sceneDirectory().isEmpty());
}

void TestSceneVariables::resolvePathWithoutASceneDirectoryReportsWhy()
{
    SceneVariables& variables = SceneVariables::instance();
    QVERIFY(variables.sceneDirectory().isEmpty());

    QString token;
    QString reason;
    // A relative path with nothing to be relative to. The cwd is exactly what
    // this must not become: the process may have been started anywhere.
    QVERIFY(variables.resolvePath(QStringLiteral("bars.mp4"), &token, &reason).isEmpty());
    QVERIFY(token.isEmpty());
    QVERIFY2(reason.contains(QStringLiteral("bars.mp4")), qPrintable(reason));
    QVERIFY2(reason.contains(QStringLiteral("scene")), qPrintable(reason));

    // An unresolved token is reported *and* left visible: save() writes back the
    // same token, and the user sees which one is missing instead of "".
    QCOMPARE(variables.resolvePath(QStringLiteral("${DAQSTER_NO_SUCH_TOKEN}/x.mp4"), &token,
                                   &reason),
             QStringLiteral("${DAQSTER_NO_SUCH_TOKEN}/x.mp4"));
    QCOMPARE(token, QStringLiteral("${DAQSTER_NO_SUCH_TOKEN}"));

    // A scene directory does not rescue a broken token — the token wins, because
    // anchoring `${X}/y` to the scene would hide the real defect.
    variables.setSceneDirectory(m_homeDir->path());
    token.clear();
    QCOMPARE(variables.resolvePath(QStringLiteral("${DAQSTER_NO_SUCH_TOKEN}/x.mp4"), &token),
             QStringLiteral("${DAQSTER_NO_SUCH_TOKEN}/x.mp4"));
    QCOMPARE(token, QStringLiteral("${DAQSTER_NO_SUCH_TOKEN}"));
}
