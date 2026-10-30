#include "test_file_playback_timing.h"

#include "FilePlaybackModel.h"
#include "FilePlaybackTiming.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QVector>

#include <cmath>

namespace {

constexpr double kSampleRate = 44100.0;
/// Mirrors FilePlaybackModel::m_chunkSize (a BYTE budget, not a frame count).
constexpr qint64 kChunkBudgetBytes = 4096;

SampledStreamDescriptor descriptorFor(SampleType type,
                                      int channels = 1,
                                      double sampleRate = kSampleRate)
{
    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = sampleRate;
    descriptor.endianness = SampleEndian::LittleEndian;
    descriptor.unit = QStringLiteral("V");
    descriptor.domain = QStringLiteral("audio");
    for (int i = 0; i < channels; ++i)
        descriptor.channels.append(StreamChannelDescriptor{QStringLiteral("ch%1").arg(i), type});
    return descriptor;
}

/// The `*.sdf.json` sidecar in the shape FilePlaybackModel expects.
QJsonObject sidecarJson(const SampledStreamDescriptor &descriptor)
{
    QJsonObject object;
    object["sampleRate"] = descriptor.sampleRate;

    QJsonArray channels;
    for (const StreamChannelDescriptor &ch : descriptor.channels) {
        QJsonObject chObj;
        chObj["name"] = ch.name;
        chObj["type"] = sampleTypeName(ch.sampleType);
        channels.append(chObj);
    }
    object["channels"] = channels;

    object["endianness"] = QStringLiteral("LittleEndian");
    object["unit"] = descriptor.unit;
    object["domain"] = descriptor.domain;
    return object;
}

/// Write `<dir>/tone.sdf` (frames * bytesPerFrame zero bytes) + its sidecar.
QString writeStreamFile(const QTemporaryDir &dir,
                        const SampledStreamDescriptor &descriptor,
                        qint64 frames,
                        const QString &baseName = QStringLiteral("tone"))
{
    const QString path = dir.filePath(baseName + QStringLiteral(".sdf"));

    QFile data(path);
    if (!data.open(QIODevice::WriteOnly))
        return QString();
    data.write(QByteArray(static_cast<int>(frames * descriptor.bytesPerFrame()), '\0'));
    data.close();

    QFile sidecar(path + QStringLiteral(".json"));
    if (!sidecar.open(QIODevice::WriteOnly))
        return QString();
    sidecar.write(QJsonDocument(sidecarJson(descriptor)).toJson());
    sidecar.close();
    return path;
}

/// Drive FilePlaybackModel through its public surface until the timer runs.
/// Returns the interval it started with, or -1 when it refused to start.
int startModelAndReadInterval(FilePlaybackModel &model, const QString &path)
{
    model.onPathChanged(path);
    model.outputConnectionCreated(QtNodes::ConnectionId{0, 0, 1, 0});
    model.onPlayRequested();

    QTimer *timer = model.findChild<QTimer *>();
    if (timer == nullptr || !timer->isActive())
        return -1;
    return timer->interval();
}

} // namespace

void TestFilePlaybackTiming::intervalIsDerivedFromFramesNotBytes()
{
    const SampledStreamDescriptor s16 = descriptorFor(SampleType::INT16);
    QCOMPARE(s16.bytesPerFrame(), 2);

    // Frames in → exact real-time milliseconds out. With bytes instead of
    // frames an S16 stream is off by exactly 2x (8820 B would read as 200 ms
    // where 4410 frames are 100 ms).
    QCOMPARE(FilePlaybackTiming::chunkIntervalMs(s16, 441 * 2), 10);
    QCOMPARE(FilePlaybackTiming::chunkIntervalMs(s16, 4410 * 2), 100);
    QCOMPARE(FilePlaybackTiming::chunkIntervalMs(s16, 8820 * 2), 200);

    // A non-integer millisecond duration is rounded to the nearest ms:
    // 1024 frames / 44100 = 23.22 ms.
    const int oneKiloFrame = FilePlaybackTiming::chunkIntervalMs(s16, 1024 * 2);
    QCOMPARE(oneKiloFrame, 23);
    QVERIFY(std::abs(double(oneKiloFrame) - 1024.0 * 1000.0 / kSampleRate) <= 1.0);
}

void TestFilePlaybackTiming::sameFrameCountGivesSameIntervalRegardlessOfSampleType()
{
    const SampledStreamDescriptor s16 = descriptorFor(SampleType::INT16);
    const SampledStreamDescriptor f32 = descriptorFor(SampleType::FLOAT32);
    QCOMPARE(f32.bytesPerFrame(), 4);

    for (const qint64 frames : {qint64(1024), qint64(2048), qint64(4410)}) {
        const int s16Ms = FilePlaybackTiming::chunkIntervalMs(s16, frames * s16.bytesPerFrame());
        const int f32Ms = FilePlaybackTiming::chunkIntervalMs(f32, frames * f32.bytesPerFrame());
        QCOMPARE(s16Ms, f32Ms);
        QCOMPARE(s16Ms, int(std::lround(double(frames) * 1000.0 / kSampleRate)));
    }

    // Stereo F32: bytesPerFrame is the SUM over channels (8), not 4 — another
    // place where a hardcoded 2/4 would silently halve/double the tempo.
    const SampledStreamDescriptor stereoF32 = descriptorFor(SampleType::FLOAT32, 2);
    QCOMPARE(stereoF32.bytesPerFrame(), 8);
    QCOMPARE(FilePlaybackTiming::chunkIntervalMs(stereoF32, 4410 * 8), 100);
}

void TestFilePlaybackTiming::unusableLayoutHasNoInterval()
{
    const SampledStreamDescriptor noChannels = descriptorFor(SampleType::INT16, 0);
    QCOMPARE(noChannels.bytesPerFrame(), 0);
    QCOMPARE(FilePlaybackTiming::chunkIntervalMs(noChannels, 4096), -1);

    const SampledStreamDescriptor noRate = descriptorFor(SampleType::INT16, 1, 0.0);
    QCOMPARE(FilePlaybackTiming::chunkIntervalMs(noRate, 4096), -1);

    const SampledStreamDescriptor valid = descriptorFor(SampleType::INT16);
    QCOMPARE(FilePlaybackTiming::chunkIntervalMs(valid, 0), -1);
    QCOMPARE(FilePlaybackTiming::chunkIntervalMs(valid, -4096), -1);
}

void TestFilePlaybackTiming::chunkBudgetIsRoundedToWholeFrames()
{
    // F32 mono: 4096 / 4 = 1024 whole frames, budget fully used.
    const SampledStreamDescriptor f32 = descriptorFor(SampleType::FLOAT32);
    QCOMPARE(FilePlaybackTiming::chunkBytesFor(f32, kChunkBudgetBytes), qint64(4096));

    // INT24 mono: 4096 / 3 = 1365 frames + 1 leftover byte — the leftover must
    // not become a split frame (it would shift every following chunk by a byte).
    const SampledStreamDescriptor int24 = descriptorFor(SampleType::INT24);
    QCOMPARE(FilePlaybackTiming::chunkBytesFor(int24, kChunkBudgetBytes), qint64(4095));

    // A budget smaller than one frame cannot produce audio at all.
    const SampledStreamDescriptor stereoF32 = descriptorFor(SampleType::FLOAT32, 2);
    QCOMPARE(FilePlaybackTiming::chunkBytesFor(stereoF32, 4), qint64(0));

    // Unusable layout reports 0 (never a bogus non-zero chunk).
    QCOMPARE(FilePlaybackTiming::chunkBytesFor(descriptorFor(SampleType::INT16, 0), 4096), qint64(0));
}

void TestFilePlaybackTiming::modelTimerUsesRealTimeInterval()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Files longer than one chunk, so the timer interval describes the blocks
    // the node will actually emit (chunk budget 4096 B). F32 mono puts 1024
    // frames in a chunk (23 ms), S16 mono puts 2048 frames (46 ms) — both are
    // the real duration of their bytes. The byte-based formula produced 92 ms
    // for BOTH (4096 / 44100 * 1000), which is the defect.
    const SampledStreamDescriptor f32 = descriptorFor(SampleType::FLOAT32);
    const SampledStreamDescriptor s16 = descriptorFor(SampleType::INT16);

    const QString f32Path = writeStreamFile(dir, f32, 4410, QStringLiteral("f32"));
    const QString s16Path = writeStreamFile(dir, s16, 4410, QStringLiteral("s16"));
    QVERIFY(!f32Path.isEmpty());
    QVERIFY(!s16Path.isEmpty());

    FilePlaybackModel f32Model;
    const int f32Interval = startModelAndReadInterval(f32Model, f32Path);
    FilePlaybackModel s16Model;
    const int s16Interval = startModelAndReadInterval(s16Model, s16Path);

    const qint64 chunkBytes = FilePlaybackTiming::chunkBytesFor(f32, kChunkBudgetBytes);
    QCOMPARE(chunkBytes, qint64(4096));

    // Interval == duration of the frames in the chunk.
    QCOMPARE(f32Interval, int(std::lround(double(chunkBytes / f32.bytesPerFrame())
                                         * 1000.0 / kSampleRate)));
    QCOMPARE(s16Interval, int(std::lround(double(chunkBytes / s16.bytesPerFrame())
                                         * 1000.0 / kSampleRate)));
    QCOMPARE(f32Interval, 23);
    QCOMPARE(s16Interval, 46);
    QVERIFY2(f32Interval != 92, "byte-based interval came back (4x too slow for F32)");
    QVERIFY2(s16Interval != 92, "byte-based interval came back (2x too slow for S16)");

    f32Model.stop();
    s16Model.stop();
}

void TestFilePlaybackTiming::modelRefusesUnusableDescriptor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Both sidecars are unusable: one declares no channels (bytesPerFrame stays
    // 0 — no frame count to derive an interval from), the other no sample rate.
    // Playback must be refused with a status message instead of starting at a
    // nonsense tempo (the guard in startPlayback is the second line of defence;
    // loadFile() rejects the descriptor first).
    SampledStreamDescriptor noChannels = descriptorFor(SampleType::INT16, 0);
    SampledStreamDescriptor noRate = descriptorFor(SampleType::INT16, 1, 0.0);

    const QVector<QPair<QString, SampledStreamDescriptor>> cases{
        {QStringLiteral("nochan"), noChannels},
        {QStringLiteral("norate"), noRate},
    };

    for (const auto &testCase : cases) {
        const QString path = writeStreamFile(dir, testCase.second, 1024, testCase.first);
        QVERIFY(!path.isEmpty());

        FilePlaybackModel model;
        QSignalSpy statusSpy(&model, &FilePlaybackModel::statusChanged);

        model.onPathChanged(path);
        model.outputConnectionCreated(QtNodes::ConnectionId{0, 0, 1, 0});
        model.onPlayRequested();

        QTimer *timer = model.findChild<QTimer *>();
        QVERIFY(timer != nullptr);
        QVERIFY2(!timer->isActive(), "playback must not start on an unusable descriptor");
        QVERIFY(!statusSpy.isEmpty());
        QVERIFY(statusSpy.last().at(0).toString().contains(QStringLiteral("Invalid descriptor")));

        model.stop();
    }
}

QTEST_GUILESS_MAIN(TestFilePlaybackTiming)
