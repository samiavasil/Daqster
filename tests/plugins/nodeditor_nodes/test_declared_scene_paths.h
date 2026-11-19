#ifndef TESTDECLAREDSCENEPATHS_H
#define TESTDECLAREDSCENEPATHS_H

#include <QtTest>

#include <memory>

class QTemporaryDir;

/// `save()` must emit the path the scene **declared**, not the one this machine
/// resolved (REQ-SW-PL-091).
///
/// The defect these tests guard against is not visible in a fresh process: the
/// node worked, the file played, and only *opening a portable scene and saving
/// it* rewrote `"${DAQSTER_DEPS}/clip.mp4"` (or `"sample_48k.wav"` next to the
/// scene) into an absolute path — a scene that is now true on exactly one
/// machine. Every case therefore goes `load()` → `save()` and compares the
/// **string**, not "is it non-empty": an absolute path is non-empty too.
///
/// Nothing here needs a device, a codec or a widget — `load()`/`save()` is all
/// that runs. Both models are compiled directly into the binary (the same
/// pattern as the schema suite next to it); `Daqster::SceneVariables` is linked,
/// so the resolver under test is the one the plugin ships.
class TestSceneDeclaredPaths : public QObject
{
    Q_OBJECT

private slots:
    /// A private token document per case: the real `~/.config` table is never
    /// read, and the ambient scene directory starts empty.
    void init();
    void cleanup();

    /// `"${DAQSTER_DEPS}/clip.mp4"` → saved as that token, used as `/deps/clip.mp4`.
    void videoFileSource_keepsTheDeclaredToken();
    /// `"sample_48k.wav"` → saved as that name (resolution has no scene dir).
    void videoFileSource_keepsARelativeName();
    /// An absolute path → saved byte for byte.
    void videoFileSource_keepsAnAbsolutePath();

    /// Same three answers for the audio source.
    void filePlayback_keepsTheDeclaredToken();
    void filePlayback_keepsARelativeName();
    void filePlayback_keepsAnAbsolutePath();

    /// `save()` → `load()` → `save()` does not drift (the second save is what a
    /// user's "open, look, save" actually writes).
    void saveLoadSaveIsIdempotent();

    /// A write through the API (the widget's field) becomes the declared text —
    /// the scene's token is not "sticky" once the user points at another file.
    void apiWriteBecomesTheDeclaredText();

    /// Empty stays empty: "no file chosen" must not become a resolved default.
    void emptyStaysEmpty();

private:
    std::unique_ptr<QTemporaryDir> m_configDir;
    QString m_document;
};

#endif // TESTDECLAREDSCENEPATHS_H
