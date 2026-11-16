#include "test_declared_scene_paths.h"

#include <Scene/SceneVariables.h>

#include <Sources/FilePlayback/FilePlaybackModel.h>
#include <Sources/Video/VideoFileSourceNode.h>

#include <QDir>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

/// The one token every case uses; its value is `/deps` so an expansion is
/// recognisable and cannot collide with a real directory.
const QString kToken = QStringLiteral("${DAQSTER_DEPS}/clip.mp4");
const QString kTokenValue = QStringLiteral("/deps");
const QString kResolved = QStringLiteral("/deps/clip.mp4");

/// A scene value that names a file **next to the scene** — the second portable
/// form REQ-SW-PL-091 allows.
const QString kRelative = QStringLiteral("sample_48k.wav");
const QString kAbsolute = QStringLiteral("/media/clips/bars.mp4");

QJsonObject withFilePath(const QString& filePath)
{
    QJsonObject object;
    object.insert(QStringLiteral("filePath"), filePath);
    return object;
}

/// What a `save()` result says for `filePath`, or a marker when the key is absent
/// (a `QCOMPARE` of markers is a readable failure, an empty string is not).
QString savedFilePath(const QJsonObject& saved)
{
    if (!saved.contains(QStringLiteral("filePath")))
        return QStringLiteral("<key absent>");
    return saved.value(QStringLiteral("filePath")).toString();
}

} // namespace

void TestSceneDeclaredPaths::init()
{
    m_configDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_configDir->isValid());

    // The real machine table is never touched: one document per case, inside a
    // temp directory (`~/Daqster/out` on this box would otherwise be readable).
    m_document = m_configDir->path() + QStringLiteral("/scene-variables.json");
    Daqster::SceneVariables::instance().setFilePath(m_document);
    Daqster::SceneVariables::instance().setPathToken(QStringLiteral("DAQSTER_DEPS"), kTokenValue);
    // A relative name means "next to the scene" — no scene adopted, so the
    // resolver refuses to answer instead of falling back to the process cwd.
    Daqster::SceneVariables::instance().setSceneDirectory(QString());
}

void TestSceneDeclaredPaths::cleanup()
{
    Daqster::SceneVariables::instance().setSceneDirectory(QString());
    // Back to the default path *before* the temp directory disappears, so the
    // next case does not read a document that is already gone.
    Daqster::SceneVariables::instance().setFilePath(QString());
    m_configDir.reset();
}

void TestSceneDeclaredPaths::videoFileSource_keepsTheDeclaredToken()
{
    VideoFileSourceNode node;
    node.load(withFilePath(kToken));

    // Runtime keeps the resolved path (the player needs a file, not a token)...
    QCOMPARE(node.filePath(), kResolved);
    // ...and save() emits the token the scene declared.
    QCOMPARE(savedFilePath(node.save()), kToken);
}

void TestSceneDeclaredPaths::videoFileSource_keepsARelativeName()
{
    VideoFileSourceNode node;
    node.load(withFilePath(kRelative));

    // No scene directory: the node refuses the relative path (empty + an error
    // message) — and still remembers what the file said.
    QVERIFY(node.filePath().isEmpty());
    QCOMPARE(savedFilePath(node.save()), kRelative);
}

void TestSceneDeclaredPaths::videoFileSource_keepsAnAbsolutePath()
{
    VideoFileSourceNode node;
    node.load(withFilePath(kAbsolute));

    QCOMPARE(node.filePath(), kAbsolute);
    QCOMPARE(savedFilePath(node.save()), kAbsolute);
}

void TestSceneDeclaredPaths::filePlayback_keepsTheDeclaredToken()
{
    FilePlaybackModel model;
    model.load(withFilePath(kToken));

    QCOMPARE(savedFilePath(model.save()), kToken);
}

void TestSceneDeclaredPaths::filePlayback_keepsARelativeName()
{
    FilePlaybackModel model;
    model.load(withFilePath(kRelative));

    QCOMPARE(savedFilePath(model.save()), kRelative);
}

void TestSceneDeclaredPaths::filePlayback_keepsAnAbsolutePath()
{
    FilePlaybackModel model;
    model.load(withFilePath(kAbsolute));

    QCOMPARE(savedFilePath(model.save()), kAbsolute);
}

void TestSceneDeclaredPaths::saveLoadSaveIsIdempotent()
{
    VideoFileSourceNode video;
    video.load(withFilePath(kToken));
    const QJsonObject firstSave = video.save();

    // The second instance starts from what the first one wrote — exactly the
    // "open the scene, save it again" path the defect lived in.
    VideoFileSourceNode reopened;
    reopened.load(firstSave);
    QCOMPARE(savedFilePath(reopened.save()), kToken);

    FilePlaybackModel playback;
    playback.load(withFilePath(kRelative));
    const QJsonObject playbackSave = playback.save();

    FilePlaybackModel reopenedPlayback;
    reopenedPlayback.load(playbackSave);
    QCOMPARE(savedFilePath(reopenedPlayback.save()), kRelative);
}

void TestSceneDeclaredPaths::apiWriteBecomesTheDeclaredText()
{
    VideoFileSourceNode video;
    video.load(withFilePath(kToken));
    video.onFilePathChanged(kAbsolute);
    QCOMPARE(savedFilePath(video.save()), kAbsolute);

    FilePlaybackModel playback;
    playback.load(withFilePath(kToken));
    playback.onPathChanged(kAbsolute);
    QCOMPARE(savedFilePath(playback.save()), kAbsolute);
}

void TestSceneDeclaredPaths::emptyStaysEmpty()
{
    VideoFileSourceNode video;
    QCOMPARE(savedFilePath(video.save()), QString());
    video.load(withFilePath(QString()));
    QCOMPARE(savedFilePath(video.save()), QString());

    FilePlaybackModel playback;
    QCOMPARE(savedFilePath(playback.save()), QString());
    playback.load(withFilePath(QString()));
    QCOMPARE(savedFilePath(playback.save()), QString());
}
