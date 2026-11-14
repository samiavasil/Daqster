#include "test_image_data.h"
#include "NodeDataTypes/ImageData.h"

#include <QImage>

#include <memory>

namespace {

/// A deterministic, non-null 64x48 image with a recognisable pixel pattern.
QImage makeTestImage()
{
    QImage image(64, 48, QImage::Format_RGB32);
    image.fill(QColor(10, 20, 30));
    image.setPixel(0, 0, qRgb(255, 0, 0));
    return image;
}

} // namespace

void ImageDataTest::typeIdIsImage()
{
    ImageData data;
    const QtNodes::NodeDataType type = data.type();
    QCOMPARE(type.id, QStringLiteral("image"));
    QCOMPARE(type.name, QStringLiteral("Image"));
}

void ImageDataTest::sharedPtrTransferKeepsTheSameBuffer()
{
    auto producer = std::make_shared<ImageData>(makeTestImage(), ImageData::Meta{});
    const uchar *before = producer->image().constBits();
    QVERIFY(before != nullptr);

    // Exactly what a connection does: the shared pointer is copied, the data
    // is not.
    std::shared_ptr<QtNodes::NodeData> transported = producer;
    auto consumed = std::static_pointer_cast<ImageData>(transported);

    QCOMPARE(consumed.get(), producer.get());
    QCOMPARE(consumed->image().constBits(), before);
}

void ImageDataTest::metadataSurvivesTransfer()
{
    ImageData::Meta meta;
    meta.prompt = QStringLiteral("котка на прозорец");
    meta.negativePrompt = QStringLiteral("blurry");
    meta.seed = 424242;
    meta.model = QStringLiteral("sd-v1-5");
    meta.mimeType = QStringLiteral("image/png");
    meta.elapsedMs = 1234;

    auto producer = std::make_shared<ImageData>(makeTestImage(), meta);
    std::shared_ptr<QtNodes::NodeData> transported = producer;
    auto consumed = std::static_pointer_cast<ImageData>(transported);

    const ImageData::Meta &got = consumed->meta();
    QCOMPARE(got.prompt, meta.prompt);
    QCOMPARE(got.negativePrompt, meta.negativePrompt);
    QCOMPARE(got.seed, meta.seed);
    QCOMPARE(got.model, meta.model);
    QCOMPARE(got.mimeType, meta.mimeType);
    QCOMPARE(got.elapsedMs, meta.elapsedMs);
}

void ImageDataTest::defaultConstructedIsEmpty()
{
    ImageData data;
    QVERIFY(data.isEmpty());
    QVERIFY(data.image().isNull());
    QCOMPARE(data.width(), 0);
    QCOMPARE(data.height(), 0);

    // "unknown" is not an error: the defaults are neutral, not sentinels that
    // force a caller to check.
    QVERIFY(data.meta().prompt.isEmpty());
    QCOMPARE(data.meta().seed, qint64(-1));
    QCOMPARE(data.meta().elapsedMs, qint64(0));
}

void ImageDataTest::imageFromNullImageIsEmpty()
{
    ImageData data{QImage(), ImageData::Meta{}};
    QVERIFY(data.isEmpty());
}

void ImageDataTest::repeatedReadsDoNotDetach()
{
    const QImage original = makeTestImage();
    // A second reference to the same buffer: the data must stay shared, i.e.
    // no read may call QImage::detach() and duplicate 12 KB of pixels.
    QImage sharedCopy = original;

    ImageData data{sharedCopy, ImageData::Meta{}};
    QVERIFY(!data.image().isDetached());

    for (int i = 0; i < 3; ++i) {
        const QImage &read = data.image();
        QCOMPARE(read.constBits(), sharedCopy.constBits());
    }

    QVERIFY(!data.image().isDetached());
    QVERIFY(!sharedCopy.isDetached());
}

void ImageDataTest::widthHeightComeFromTheImage()
{
    ImageData data{makeTestImage(), ImageData::Meta{}};
    QVERIFY(!data.isEmpty());
    QCOMPARE(data.width(), 64);
    QCOMPARE(data.height(), 48);
    QCOMPARE(data.width(), data.image().width());
    QCOMPARE(data.height(), data.image().height());
}

// No QTEST_GUILESS_MAIN here: the ImageData test class shares the
// nodeditor_nodes test binary whose main lives in test_main.cpp.
