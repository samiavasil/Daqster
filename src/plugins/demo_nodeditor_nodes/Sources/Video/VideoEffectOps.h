#ifndef VIDEOEFFECTOPS_H
#define VIDEOEFFECTOPS_H

#include <QImage>
#include <QString>
#include <QVector>

#include <functional>

/**
 * @brief Effect parameter set shared by the CPU and GPU backends.
 *
 * brightness: -100..+100 (0 = unchanged), contrast: 0..200 (100 = unchanged),
 * flipHorizontal: flip direction for the flip effect (CPU path; the GPU path
 * flips vertically through the u_flipY shader uniform).
 */
struct EffectParams
{
    int brightness = 0;
    int contrast = 100;
    bool flipHorizontal = true;
};

/**
 * @brief Description of a single video effect (REQ-SW-PL-028).
 *
 * One node = one effect: each effect is registered as its own node instance.
 * The backend is chosen at runtime per effect + GL detection — CpuOnly effects
 * always run on the CPU, GpuOrCpu effects run on the GPU when hardware GL is
 * available and fall back to the CPU otherwise.
 */
struct EffectSpec
{
    enum class Backend { CpuOnly, GpuOrCpu };

    QString id;
    QString displayName;
    Backend backend = Backend::GpuOrCpu;
    /// CPU implementation (delegates to VideoTransformOps).
    std::function<QImage(const QImage &, const EffectParams &)> cpuApply;
    /// GLSL statements injected into the effect fragment shader, operating on
    /// the local `vec3 rgb` variable (may be empty — e.g. flip via u_flipY).
    QString glslBody;
};

namespace VideoEffectOps {

/// All registered effects (7): brightness, contrast, grayscale, invert, sepia,
/// channelSwap, flip.
QVector<EffectSpec> allSpecs();

/// Look up an effect by id; returns an empty (invalid) EffectSpec when unknown.
EffectSpec specFor(const QString &id);

} // namespace VideoEffectOps

#endif // VIDEOEFFECTOPS_H