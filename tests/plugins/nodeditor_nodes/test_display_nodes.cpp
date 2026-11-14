#include "test_display_nodes.h"

#include "Displays/FrameWindowDisplay/FrameWindowDisplayNode.h"
#include "Displays/ImageDisplay/ImageDisplayNode.h"
#include "Displays/NumberDisplay/NumberDisplayDataModel.h"
#include "Displays/TextDisplay/TextDisplayNode.h"
#include "NodeDataTypes/EmbeddingData.h"
#include "NodeDataTypes/FrameWindowData.h"
#include "NodeDataTypes/ImageData.h"
#include "NodeDataTypes/NumericType.h"
#include "NodeDataTypes/TextData.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "NodeDataTypes/VideoTextureHandle.h"

#include <QColor>

#include <QImage>
#include <QJsonObject>
#include <QSignalSpy>
#include <QVideoFrame>

#include <memory>
#include <vector>

using QtNodes::NodeData;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

QImage makeImage(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(QColor(20, 40, 60));
    image.setPixel(0, 0, qRgb(255, 0, 0));
    return image;
}

std::shared_ptr<VideoFrameData> makeFrame(int tint)
{
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(QColor(tint % 256, (tint * 7) % 256, (tint * 13) % 256));
    const QVideoFrame frame(image);
    if (!frame.isValid())
        return nullptr;
    return std::make_shared<VideoFrameData>(frame);
}

} // namespace

// ── Contract: a display is a consumer ───────────────────────────────────────

void TestDisplayNodes::textDisplayIsAConsumer()
{
    TextDisplayNode node;
    QCOMPARE(node.nPorts(PortType::In), 1u);
    QCOMPARE(node.nPorts(PortType::Out), 0u);
    QCOMPARE(node.nPorts(PortType::None), 0u);
    QCOMPARE(node.dataType(PortType::In, 0).id, QStringLiteral("text"));
    QCOMPARE(node.outData(0), nullptr);
    QCOMPARE(node.embeddedWidget(), nullptr);
    QCOMPARE(node.name(), QStringLiteral("TextDisplay"));
}

void TestDisplayNodes::imageDisplayIsAConsumer()
{
    ImageDisplayNode node;
    QCOMPARE(node.nPorts(PortType::In), 1u);
    QCOMPARE(node.nPorts(PortType::Out), 0u);
    QCOMPARE(node.dataType(PortType::In, 0).id, QStringLiteral("image"));
    QCOMPARE(node.outData(0), nullptr);
    QCOMPARE(node.embeddedWidget(), nullptr);
    QCOMPARE(node.name(), QStringLiteral("ImageDisplay"));
}

void TestDisplayNodes::frameWindowDisplayIsAConsumer()
{
    FrameWindowDisplayNode node;
    QCOMPARE(node.nPorts(PortType::In), 1u);
    QCOMPARE(node.nPorts(PortType::Out), 0u);
    QCOMPARE(node.dataType(PortType::In, 0).id, QStringLiteral("frame-window"));
    QCOMPARE(node.outData(0), nullptr);
    QCOMPARE(node.embeddedWidget(), nullptr);
    QCOMPARE(node.name(), QStringLiteral("FrameWindowDisplay"));
}

// ── TextDisplay ─────────────────────────────────────────────────────────────

void TestDisplayNodes::textAccumulatesCounters()
{
    TextDisplayNode node;
    QCOMPARE(node.messageCount(), qint64(0));

    const QString first = QStringLiteral("hello");
    const QString second = QStringLiteral("действително дълъг");
    node.setInData(std::make_shared<TextData>(first), 0);
    node.setInData(std::make_shared<TextData>(second), 0);

    QCOMPARE(node.messageCount(), qint64(2));
    QCOMPARE(node.characterCount(), qint64(first.size() + second.size()));
    QCOMPARE(node.lastText(), second);
}

void TestDisplayNodes::textSnapshotTruncatesPreviewOnly()
{
    const QString text = QString(500, QLatin1Char('x'));
    const TextDisplaySnapshot snapshot = TextDisplayNode::buildSnapshot(text, 1, 500, 12, 10);

    QCOMPARE(snapshot.preview.size(), 11); // 10 chars + the ellipsis
    QVERIFY(snapshot.preview.endsWith(QStringLiteral("…")));
    QVERIFY(snapshot.truncated);
    QCOMPARE(snapshot.full, text);         // the tooltip keeps everything
    QCOMPARE(snapshot.messages, qint64(1));
    QCOMPARE(snapshot.characters, qint64(500));
    QCOMPARE(snapshot.msSinceLast, qint64(12));
}

void TestDisplayNodes::textSnapshotCountsLines()
{
    QCOMPARE(TextDisplayNode::buildSnapshot(QString(), 0, 0, -1).lines, 0);
    QCOMPARE(TextDisplayNode::buildSnapshot(QStringLiteral("one"), 1, 3, 0).lines, 1);
    QCOMPARE(TextDisplayNode::buildSnapshot(QStringLiteral("a\nb\nc"), 1, 5, 0).lines, 3);
    // A short text is not marked as truncated and keeps the very same string.
    const TextDisplaySnapshot shortOne = TextDisplayNode::buildSnapshot(QStringLiteral("ok"), 1, 2, 0);
    QVERIFY(!shortOne.truncated);
    QCOMPARE(shortOne.preview, QStringLiteral("ok"));
}

void TestDisplayNodes::textRejectsForeignData()
{
    // A foreign payload must not touch the counters, and must not crash.
    TextDisplayNode node;
    node.setInData(std::make_shared<NumericType<double>>(1.5), 0);
    QCOMPARE(node.messageCount(), qint64(0));
    QCOMPARE(node.validationState()._state, QtNodes::NodeValidationState::State::Warning);

    node.setInData(nullptr, 0);
    QCOMPARE(node.validationState()._state, QtNodes::NodeValidationState::State::Warning);
}

void TestDisplayNodes::textSnapshotIsDelivered()
{
    TextDisplayNode node;
    QSignalSpy spy(&node, &TextDisplayNode::snapshotReady);
    QVERIFY(spy.isValid());

    node.setInData(std::make_shared<TextData>(QStringLiteral("stream line")), 0);

    // The 30 Hz ticket submits to the shared ComputePool; the result arrives
    // through a queued invocation, so it needs the event loop to turn.
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
    const auto snapshot = spy.first().first().value<TextDisplaySnapshot>();
    QCOMPARE(snapshot.preview, QStringLiteral("stream line"));
    QCOMPARE(snapshot.messages, qint64(1));
    QVERIFY(snapshot.msSinceLast >= 0);
}

// ── ImageDisplay ────────────────────────────────────────────────────────────

void TestDisplayNodes::imageSnapshotCarriesGeometryAndFormat()
{
    ImageData::Meta meta;
    meta.mimeType = QStringLiteral("image/png");
    meta.model = QStringLiteral("sdxl");
    meta.elapsedMs = 1234;
    const ImageData data(makeImage(64, 48), meta);

    const ImageDisplaySnapshot snapshot =
        ImageDisplayNode::buildSnapshot(data.image(), meta.mimeType, meta.model,
                                        meta.elapsedMs, 3, 7);
    QCOMPARE(snapshot.width, 64);
    QCOMPARE(snapshot.height, 48);
    QCOMPARE(snapshot.format, QStringLiteral("RGB32"));
    QCOMPARE(snapshot.mimeType, QStringLiteral("image/png"));
    QCOMPARE(snapshot.model, QStringLiteral("sdxl"));
    QCOMPARE(snapshot.metaElapsedMs, qint64(1234));
    QCOMPARE(snapshot.messages, qint64(3));
    QCOMPARE(snapshot.msSinceLast, qint64(7));
    QVERIFY(!snapshot.downscaled); // 64×48 already fits 320×180
}

void TestDisplayNodes::imageThumbnailIsBounded()
{
    const ImageData data(makeImage(1920, 1080), ImageData::Meta{});
    const ImageDisplaySnapshot snapshot =
        ImageDisplayNode::buildSnapshot(data.image(), QString(), QString(), 0, 1, 0);

    QVERIFY(snapshot.downscaled);
    QVERIFY(snapshot.thumbnail.width() <= ImageDisplayNode::kPreviewWidth);
    QVERIFY(snapshot.thumbnail.height() <= ImageDisplayNode::kPreviewHeight);
    // Aspect ratio preserved (16:9 within a pixel of rounding).
    QVERIFY(qAbs(snapshot.thumbnail.width() * 9 - snapshot.thumbnail.height() * 16) <= 16);
    // The source geometry is still reported, not the thumbnail's.
    QCOMPARE(snapshot.width, 1920);
    QCOMPARE(snapshot.height, 1080);
}

void TestDisplayNodes::imageThumbnailDoesNotAliasTheSource()
{
    const ImageData data(makeImage(640, 480), ImageData::Meta{});
    const ImageDisplaySnapshot snapshot =
        ImageDisplayNode::buildSnapshot(data.image(), QString(), QString(), 0, 1, 0);

    // Different buffer: the display must not pin the producer's pixels.
    QVERIFY(snapshot.thumbnail.constBits() != data.image().constBits());
    QVERIFY(snapshot.thumbnail.cacheKey() != data.image().cacheKey());
}

void TestDisplayNodes::imageNodeDoesNotMutateThePayload()
{
    ImageData data(makeImage(64, 48), ImageData::Meta{});
    const qint64 before = data.image().cacheKey();
    const QRgb pixelBefore = data.image().pixel(0, 0);

    ImageDisplayNode node;
    node.setInData(std::make_shared<ImageData>(data.image(), data.meta()), 0);
    // Let the worker pass run and deliver.
    QTest::qWait(120);
    node.stop();

    QCOMPARE(data.image().cacheKey(), before);
    QCOMPARE(data.image().pixel(0, 0), pixelBefore);
}

void TestDisplayNodes::imageFormatNamesAreStable()
{
    QCOMPARE(ImageDisplayNode::formatName(QImage::Format_RGB32), QStringLiteral("RGB32"));
    QCOMPARE(ImageDisplayNode::formatName(QImage::Format_ARGB32), QStringLiteral("ARGB32"));
    QCOMPARE(ImageDisplayNode::formatName(QImage::Format_Grayscale8), QStringLiteral("grayscale8"));
    QCOMPARE(ImageDisplayNode::formatName(QImage::Format_Invalid), QStringLiteral("invalid"));
}

// ── FrameWindowDisplay ──────────────────────────────────────────────────────

void TestDisplayNodes::frameWindowSnapshotReportsMetadata()
{
    QVector<FrameWindowData::Item> items;
    for (int i = 0; i < 3; ++i)
        items.append(FrameWindowData::Item{makeFrame(i), 100 * (i + 1)});
    const FrameWindowData window(items);

    const FrameWindowDisplaySnapshot snapshot =
        FrameWindowDisplayNode::buildSnapshot(window, 2, 30);
    QCOMPARE(snapshot.count, 3);
    QCOMPARE(snapshot.firstTimestampMs, qint64(100));
    QCOMPARE(snapshot.lastTimestampMs, qint64(300));
    QCOMPARE(snapshot.spanMs, qint64(200));
    QCOMPARE(snapshot.messages, qint64(2));
    QCOMPARE(snapshot.msSinceLast, qint64(30));
    QVERIFY(!snapshot.metadataOnly);
    QCOMPARE(snapshot.skipped, 0);
}

void TestDisplayNodes::frameWindowKeepsAtMostFourThumbnails()
{
    QVector<FrameWindowData::Item> items;
    for (int i = 0; i < 6; ++i)
        items.append(FrameWindowData::Item{makeFrame(i), i});
    const FrameWindowData window(items);

    const FrameWindowDisplaySnapshot snapshot =
        FrameWindowDisplayNode::buildSnapshot(window, 1, 0);
    QCOMPARE(snapshot.thumbnails.size(), FrameWindowDisplayNode::kMaxThumbnails);
    for (const QImage &thumb : snapshot.thumbnails) {
        QVERIFY(!thumb.isNull());
        QVERIFY(thumb.width() <= FrameWindowDisplayNode::kThumbnailWidth);
    }
}

void TestDisplayNodes::frameWindowWithoutCpuFramesIsMetadataOnly()
{
    // GPU-resident payload: no CPU pixels, and asImage() is a GL readback that
    // must never run off the GUI thread — report it instead of faking a thumb.
    VideoTextureHandle handle;
    handle.width = 64;
    handle.height = 48;
    handle.rgba = true;
    QVector<FrameWindowData::Item> items;
    items.append(FrameWindowData::Item{VideoFrameData::fromTexture(handle), 10});
    items.append(FrameWindowData::Item{nullptr, 20});
    const FrameWindowData window(items);

    const FrameWindowDisplaySnapshot snapshot =
        FrameWindowDisplayNode::buildSnapshot(window, 1, 0);
    QCOMPARE(snapshot.count, 2);
    QVERIFY(snapshot.thumbnails.isEmpty());
    QCOMPARE(snapshot.skipped, 2);
    QVERIFY(snapshot.metadataOnly);
    QCOMPARE(snapshot.lastTimestampMs, qint64(20));
}

// ── NumberDisplay + EmbeddingData ───────────────────────────────────────────

void TestDisplayNodes::embeddingSummaryReportsStatistics()
{
    const std::vector<float> values{0.5f, -0.25f, 1.0f, 0.75f};
    const QString summary = NumberDisplayDataModel::embeddingSummary(values, 2);

    QVERIFY(summary.startsWith(QStringLiteral("dim 4")));
    QVERIFY(summary.contains(QStringLiteral("min -0.25")));
    QVERIFY(summary.contains(QStringLiteral("max 1")));
    QVERIFY(summary.contains(QStringLiteral("mean 0.5")));
    // Exactly the first two components, then an ellipsis marker.
    QVERIFY(summary.contains(QStringLiteral("0.5  0.75")) || summary.contains(QStringLiteral("0.5  -0.25")));
    QVERIFY(summary.contains(QStringLiteral("…")));
}

void TestDisplayNodes::embeddingPortTypeFollowsTheSelector()
{
    // NumericType's port id is typeid(T).name(): "d" for double, "i" for int —
    // that is the pre-existing contract, kept byte-identical here.
    NumberDisplayDataModel model;
    QCOMPARE(model.dataType(PortType::In, 0).id, QStringLiteral("d"));

    model.onTypeChanged(2); // embedding
    QCOMPARE(model.dataType(PortType::In, 0).id, QStringLiteral("embedding"));

    model.onTypeChanged(1); // int — unchanged behaviour
    QCOMPARE(model.dataType(PortType::In, 0).id, QStringLiteral("i"));

    model.onTypeChanged(0); // double — unchanged behaviour
    QCOMPARE(model.dataType(PortType::In, 0).id, QStringLiteral("d"));
}

void TestDisplayNodes::embeddingEmptyIsRejected()
{
    NumberDisplayDataModel model;
    model.onTypeChanged(2);
    QSignalSpy spy(&model, &NumberDisplayDataModel::displayTextChanged);

    model.setInData(std::make_shared<EmbeddingData>(std::vector<float>{0.1f, 0.2f}), 0);
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.first().first().toString().startsWith(QStringLiteral("dim 2")));

    model.setInData(std::make_shared<EmbeddingData>(), 0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.last().first().toString(), QString());
}

void TestDisplayNodes::numberDisplaySaveLoadRoundTrip()
{
    NumberDisplayDataModel model;
    model.onTypeChanged(2);
    const QJsonObject saved = model.save();
    QCOMPARE(saved.value(QStringLiteral("type")).toString(), QStringLiteral("embedding"));

    NumberDisplayDataModel restored;
    restored.load(saved);
    QCOMPARE(restored.dataType(PortType::In, 0).id, QStringLiteral("embedding"));

    // Old files (int / double / missing) keep loading as before.
    QJsonObject legacy;
    legacy["type"] = QStringLiteral("int");
    restored.load(legacy);
    QCOMPARE(restored.dataType(PortType::In, 0).id, QStringLiteral("i"));
    restored.load(QJsonObject{});
    QCOMPARE(restored.dataType(PortType::In, 0).id, QStringLiteral("d"));
}

void TestDisplayNodes::numberDisplayDoubleBehaviourIsUnchanged()
{
    NumberDisplayDataModel model;
    QSignalSpy spy(&model, &NumberDisplayDataModel::displayTextChanged);
    QVERIFY(spy.isValid());

    const auto value = std::make_shared<NumericType<double>>(2.5);
    model.setInData(value, 0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), value->numberAsText());

    // A foreign payload still clears the label, exactly as before.
    model.setInData(std::make_shared<TextData>(QStringLiteral("text")), 0);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.last().first().toString(), QString());
}
