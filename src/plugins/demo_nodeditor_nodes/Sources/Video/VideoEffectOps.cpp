#include "VideoEffectOps.h"

#include "VideoTransformOps.h"

namespace VideoEffectOps {

namespace {

EffectSpec makeSpec(const QString &id, const QString &displayName,
                    EffectSpec::Backend backend,
                    std::function<QImage(const QImage &, const EffectParams &)> cpuApply,
                    const QString &glslBody)
{
    EffectSpec spec;
    spec.id = id;
    spec.displayName = displayName;
    spec.backend = backend;
    spec.cpuApply = std::move(cpuApply);
    spec.glslBody = glslBody;
    return spec;
}

} // namespace

QVector<EffectSpec> allSpecs()
{
    QVector<EffectSpec> specs;

    specs.append(makeSpec(
        QStringLiteral("brightness"), QStringLiteral("Brightness"),
        EffectSpec::Backend::GpuOrCpu,
        [](const QImage &img, const EffectParams &p) {
            return VideoTransformOps::brightness(img, p.brightness);
        },
        QStringLiteral("  rgb = rgb + vec3(u_brightness);\n")));

    specs.append(makeSpec(
        QStringLiteral("contrast"), QStringLiteral("Contrast"),
        EffectSpec::Backend::GpuOrCpu,
        [](const QImage &img, const EffectParams &p) {
            return VideoTransformOps::contrast(img, p.contrast);
        },
        QStringLiteral("  rgb = (rgb - vec3(0.5)) * u_contrast + vec3(0.5);\n")));

    specs.append(makeSpec(
        QStringLiteral("grayscale"), QStringLiteral("Grayscale"),
        EffectSpec::Backend::GpuOrCpu,
        [](const QImage &img, const EffectParams &) {
            return VideoTransformOps::grayscale(img);
        },
        QStringLiteral(
            "  float g = dot(rgb, vec3(0.299, 0.587, 0.114));\n"
            "  rgb = vec3(g);\n")));

    specs.append(makeSpec(
        QStringLiteral("invert"), QStringLiteral("Invert"),
        EffectSpec::Backend::GpuOrCpu,
        [](const QImage &img, const EffectParams &) {
            return VideoTransformOps::invert(img);
        },
        QStringLiteral("  rgb = vec3(1.0) - rgb;\n")));

    specs.append(makeSpec(
        QStringLiteral("sepia"), QStringLiteral("Sepia"),
        EffectSpec::Backend::GpuOrCpu,
        [](const QImage &img, const EffectParams &) {
            return VideoTransformOps::sepia(img);
        },
        // Column-major mat3 matching the CPU sepia coefficients:
        // outR = 0.393r + 0.769g + 0.189b, outG = 0.349r + 0.686g + 0.168b,
        // outB = 0.272r + 0.534g + 0.131b.
        QStringLiteral(
            "  rgb = mat3(0.393, 0.349, 0.272,\n"
            "             0.769, 0.686, 0.534,\n"
            "             0.189, 0.168, 0.131) * rgb;\n")));

    specs.append(makeSpec(
        QStringLiteral("channelSwap"), QStringLiteral("RGB Channel Swap"),
        EffectSpec::Backend::GpuOrCpu,
        [](const QImage &img, const EffectParams &) {
            return VideoTransformOps::swapRedBlue(img);
        },
        QStringLiteral("  rgb = rgb.bgr;\n")));

    specs.append(makeSpec(
        QStringLiteral("flip"), QStringLiteral("Flip"),
        EffectSpec::Backend::GpuOrCpu,
        [](const QImage &img, const EffectParams &p) {
            return VideoTransformOps::flip(img, p.flipHorizontal);
        },
        // Empty body: the GPU flip is done through the u_flipY shader uniform
        // (vertical flip of the texture coordinate).
        QString()));

    // CPU-only effects (REQ-SW-PL-028 AC 3): always run on the CPU regardless
    // of GL availability. The OpenCV-backed ones (gaussianBlur, canny,
    // threshold) are only registered when HAVE_OPENCV is defined.
    specs.append(makeSpec(
        QStringLiteral("blur"), QStringLiteral("Blur"),
        EffectSpec::Backend::CpuOnly,
        [](const QImage &img, const EffectParams &p) {
            return VideoTransformOps::blur(img, p.blurRadius);
        },
        QString()));

#ifdef HAVE_OPENCV
    specs.append(makeSpec(
        QStringLiteral("gaussianBlur"), QStringLiteral("Gaussian Blur"),
        EffectSpec::Backend::CpuOnly,
        [](const QImage &img, const EffectParams &p) {
            return VideoTransformOps::gaussianBlur(img, p.gaussianKernel);
        },
        QString()));

    specs.append(makeSpec(
        QStringLiteral("canny"), QStringLiteral("Canny Edges"),
        EffectSpec::Backend::CpuOnly,
        [](const QImage &img, const EffectParams &p) {
            return VideoTransformOps::canny(img, p.cannyLow, p.cannyHigh);
        },
        QString()));

    specs.append(makeSpec(
        QStringLiteral("threshold"), QStringLiteral("Threshold"),
        EffectSpec::Backend::CpuOnly,
        [](const QImage &img, const EffectParams &p) {
            return VideoTransformOps::threshold(img, p.thresholdValue);
        },
        QString()));
#endif // HAVE_OPENCV

    return specs;
}

EffectSpec specFor(const QString &id)
{
    const QVector<EffectSpec> specs = allSpecs();
    for (const EffectSpec &spec : specs) {
        if (spec.id == id)
            return spec;
    }
    return EffectSpec();
}

} // namespace VideoEffectOps