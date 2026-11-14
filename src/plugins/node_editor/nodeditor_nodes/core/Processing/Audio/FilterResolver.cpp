// SPDX-License-Identifier: MIT

#include "Processing/Audio/FilterResolver.h"

#include "Scene/SceneLayout.h"
#include "Scene/SceneVariables.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>

namespace {

/// The file the built-in default is designed from, for the status line.
QString defaultRef(AudioFilterKind kind, int up, int down)
{
    return kind == AudioFilterKind::Decimation
        ? QStringLiteral("default/anti-alias ÷%1").arg(down)
        : QStringLiteral("default/anti-alias %1/%2").arg(up).arg(down);
}

/// Read a kernel from a file that is known to exist and parse it.
///
/// Both loaders are used deliberately: a `.json` document carries provenance
/// (`ref`, `sha256`, declared delay) that must be verified, while a bare-taps
/// file is "I designed this elsewhere and I have the numbers". A `.json` name
/// with only a `taps` array is still a JSON document, so the choice is by
/// suffix, never by content sniffing.
AudioFilterResolution loadKernelFile(const QString& path, const QString& source)
{
    AudioFilterResolution result;
    result.source = source;

    QString reason;
    FirCoefficients coefficients;
    if (QFileInfo(path).suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            result.error = QStringLiteral("%1: не мога да отворя файла (%2)")
                               .arg(path, file.errorString());
            return result;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            result.error = QStringLiteral("%1: не е JSON обект (%2)")
                               .arg(path, parseError.errorString());
            return result;
        }
        coefficients = FirCoefficients::fromJson(document.object(), &reason,
                                                 QFileInfo(path).absolutePath());
    } else {
        coefficients = FirCoefficients::fromTapsFile(path, &reason);
    }

    if (coefficients.isEmpty()) {
        result.error = QStringLiteral("%1: %2").arg(path, reason);
        return result;
    }
    if (!coefficients.validate(&reason)) {
        result.error = QStringLiteral("%1: %2").arg(path, reason);
        return result;
    }

    result.coefficients = coefficients;
    result.ok = true;
    return result;
}

/// Look a `filterRef` up under `<DEPS>/filters/`.
///
/// Returns an empty `path` plus a human-readable `miss` note when the ref
/// cannot be satisfied — a miss is not an error, it falls through to the next
/// level with the reason recorded.
QString refFilePath(const QString& filterRef, QString* miss)
{
    const QString layoutDir = Daqster::SceneLayout::fromMachine().filtersDir();
    if (layoutDir.isEmpty()) {
        *miss = QStringLiteral("ref '%1' (няма DAQSTER_DEPS)").arg(filterRef);
        return QString();
    }

    QString fileName = filterRef.trimmed();
    if (QFileInfo(fileName).suffix().isEmpty())
        fileName += QStringLiteral(".json");

    const QString candidate = QDir(layoutDir).filePath(fileName);
    if (!QFileInfo::exists(candidate)) {
        *miss = QStringLiteral("ref '%1' липсва в %2").arg(filterRef, layoutDir);
        return QString();
    }
    return candidate;
}

} // namespace

QString AudioFilterResolution::describe() const
{
    if (!ok)
        return error.isEmpty() ? QStringLiteral("коефициентите не са налични") : error;

    // Eight hex digits is the prefix a human compares; the full hash is on the
    // node if it is ever needed (`coefficients.sha256OfTaps()`).
    return QStringLiteral("%1 · sha256 %2")
        .arg(source, coefficients.sha256OfTaps().left(8));
}

AudioFilterResolution resolveAudioFilter(AudioFilterKind kind, int up, int down,
                                         const QString& filterRef,
                                         const QString& filterPath)
{
    QString note;
    const QString ref = filterRef.trimmed();

    // 1. Named filter under <DEPS>/filters/.
    if (!ref.isEmpty()) {
        QString miss;
        const QString path = refFilePath(ref, &miss);
        if (!path.isEmpty()) {
            return loadKernelFile(path, QStringLiteral("filters/%1").arg(QFileInfo(path).fileName()));
        }
        note = miss;
    }

    // 2. Explicit path, resolved the same way a scene's file fields are.
    const QString rawPath = filterPath.trimmed();
    if (!rawPath.isEmpty()) {
        QString reason;
        const QString path = Daqster::SceneVariables::instance().resolvePath(rawPath, nullptr, &reason);
        if (path.isEmpty()) {
            AudioFilterResolution result;
            result.source = QStringLiteral("path:%1").arg(rawPath);
            result.error = QStringLiteral("%1: %2").arg(rawPath, reason);
            return result;
        }
        if (!QFileInfo::exists(path)) {
            AudioFilterResolution result;
            result.source = QStringLiteral("path:%1").arg(path);
            result.error = QStringLiteral("%1: файлът не съществува").arg(path);
            return result;
        }
        return loadKernelFile(path, QStringLiteral("path:%1").arg(path));
    }

    // 3. Built-in default — the numbers a scene without a filter runs with.
    AudioFilterResolution result;
    result.source = defaultRef(kind, up, down);
    if (!note.isEmpty())
        result.source += QStringLiteral(" (%1 → подразбиране)").arg(note);

    // The prototype of a rational resampler lives at `up ×` the input rate, so
    // it needs at least `up` taps just to have a coefficient in every phase —
    // with fewer, whole phases are zero and the output is silence punctuated by
    // spikes (measured with the 64-tap default at 160/147). Sixteen taps per
    // phase is a floor, not a ceiling: the wrappers raise it further to the
    // Kaiser estimate for the 0.90 cutoff rule (`tapsForTransition()`), and
    // `kMaxTaps` caps the cost for extreme ratios.
    int taps = FirCoefficients::kDefaultTaps;
    if (kind == AudioFilterKind::Resampling)
        taps = std::min(std::max(taps, 16 * up), FirCoefficients::kMaxTaps);

    result.coefficients = kind == AudioFilterKind::Decimation
        ? FirCoefficients::antiAliasForDecimation(down, taps)
        : FirCoefficients::antiAliasForResampling(up, down, taps);

    QString reason;
    if (!result.coefficients.validate(&reason)) {
        result.error = QStringLiteral("%1: %2").arg(result.source, reason);
        return result;
    }
    result.ok = true;
    return result;
}
