#include "Dsp/FirCoefficients.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLatin1String>
#include <QRegularExpression>
#include <QtNumeric>

#include <algorithm>
#include <cmath>

namespace {

/// π as a literal: `M_PI` is not part of the standard and MSVC needs a define.
constexpr double kPi = 3.14159265358979323846;

/// sin(πx)/(πx) with the removable singularity at 0 filled in.
double sincNormalized(double x)
{
    if (std::abs(x) < 1e-12)
        return 1.0;
    const double px = kPi * x;
    return std::sin(px) / px;
}

/// Kaiser window of `n` taps with shape `beta`, evaluated at index `i`.
double kaiserAt(int i, int n, double beta)
{
    if (n <= 1)
        return 1.0;
    const double centre = 0.5 * double(n - 1);
    const double r = (double(i) - centre) / centre;  // −1 … 1
    const double arg = beta * std::sqrt(std::max(0.0, 1.0 - r * r));
    return daqsterBesselI0(arg) / daqsterBesselI0(beta);
}

/// Canonical text form of the taps: 17 significant digits round-trips a double
/// exactly, so two loads of the same numbers hash identically no matter how the
/// source spelled them ("0.5" vs "0.50000000000000000").
QByteArray tapsAsCanonicalBytes(const QVector<double> &taps)
{
    QByteArray bytes;
    bytes.reserve(taps.size() * 20);
    for (int i = 0; i < taps.size(); ++i) {
        if (i > 0)
            bytes.append('\n');
        bytes.append(QByteArray::number(taps.at(i), 'g', 17));
    }
    return bytes;
}

QString defaultRef(int taps, double cutoffRatio, double beta)
{
    return QStringLiteral("default/kaiser-%1-b%2-c%3")
        .arg(taps)
        .arg(beta, 0, 'g', 4)
        .arg(cutoffRatio, 0, 'g', 6);
}

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

} // namespace

double daqsterBesselI0(double x)
{
    // Series form: I0(x) = Σ (x²/4)^k / (k!)². Converges in a handful of terms
    // for the β range a window uses (0 … 20); the loop bound is a safety net.
    const double y = 0.25 * x * x;
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 64; ++k) {
        term *= y / double(k) / double(k);
        sum += term;
        if (term < 1e-18 * sum)
            break;
    }
    return sum;
}

FirCoefficients FirCoefficients::makeDefault(int taps, double cutoffRatio, double beta)
{
    FirCoefficients result;
    const int n = std::clamp(taps, kMinTaps, kMaxTaps);
    const double ratio = (cutoffRatio > 0.0 && cutoffRatio < 1.0) ? cutoffRatio
                                                                  : kDefaultCutoffRatio;
    const double b = (beta > 0.0) ? beta : kDefaultBeta;

    // −6 dB point in cycles/sample: the Nyquist is 0.5, so 0.90·Nyquist = 0.45.
    const double fc = 0.5 * ratio;
    const double centre = 0.5 * double(n - 1);

    result.m_taps.resize(n);
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double h = 2.0 * fc * sincNormalized(2.0 * fc * (double(i) - centre))
                         * kaiserAt(i, n, b);
        result.m_taps[i] = h;
        sum += h;
    }
    if (sum != 0.0) {
        for (double &h : result.m_taps)
            h /= sum;
    }

    result.m_ref = defaultRef(n, ratio, b);
    result.m_cutoff = ratio;
    result.m_beta = b;
    result.m_delay = result.delaySamples();
    return result;
}

FirCoefficients FirCoefficients::antiAliasForDecimation(int M, int taps, double factor,
                                                        double beta)
{
    // The filter runs at the INPUT rate; the Nyquist that matters is the lower
    // (post-decimation) one, so the cutoff in input-rate terms is factor / M.
    const int m = std::max(1, M);
    const double f = (factor > 0.0 && factor < 1.0) ? factor : kDefaultCutoffRatio;

    // Transition, input-rate cycles/sample: the −6 dB point is at f·(fs/2)/M and
    // the stopband edge at (fs/2)/M, and Kaiser's Δf spans both sides of the
    // cutoff — so the full width is 2·(1 − f)·0.5/M = (1 − f)/M.
    const int needed = tapsForTransition((1.0 - f) / double(m), beta);
    return makeDefault(std::min(std::max(taps, needed), kMaxTaps), f / double(m), beta);
}

FirCoefficients FirCoefficients::antiAliasForResampling(int up, int down, int taps,
                                                        double factor, double beta)
{
    // The prototype runs at up× the input rate. The design Nyquist is up× the
    // input's, and the lower of the two rates is min(up, down) of the input's,
    // so the passband edge is factor·min(up,down)/(up·down) of the design
    // Nyquist. `up·down` (not `up`) is what makes every polyphase branch sum to
    // 1/up; with the wider kernel the branches see different DC gains.
    const int l = std::max(1, up);
    const int m = std::max(1, down);
    const double f = (factor > 0.0 && factor < 1.0) ? factor : kDefaultCutoffRatio;
    const double transition = (1.0 - f) * double(std::min(l, m)) / (double(l) * double(m));
    const double ratio = f * double(std::min(l, m)) / (double(l) * double(m));

    // Every phase must own at least one tap: with taps < up, whole phases are
    // empty and their output samples are exactly zero. The Kaiser estimate then
    // widens the prototype to the 0.90 transition (capped for extreme ratios).
    const int needed = std::max(l, tapsForTransition(transition, beta));
    const int effectiveTaps = std::min(std::max(taps, needed), kMaxTaps);
    return makeDefault(effectiveTaps, ratio, beta);
}

int FirCoefficients::tapsForTransition(double transitionCyclesPerSample, double beta)
{
    // Kaiser: Δω = (A − 8)/(2.285·N) rad/sample with Δω = 2π·Δf/fs, and a
    // Kaiser window of shape β reaches a stopband level of A = β/0.1102 + 8.7 dB.
    // Solving for N with A = A(β) keeps the pair consistent — the deeper window
    // is exactly why it needs more taps for the same edge.
    const double b = (beta > 0.0) ? beta : kDefaultBeta;
    const double delta = 2.0 * kPi * transitionCyclesPerSample;
    if (!(delta > 0.0))
        return kMaxTaps; // a zero-width transition cannot be realised

    const double attenuation = b / 0.1102 + 8.7;
    const int estimate = int(std::ceil((attenuation - 8.0) / (2.285 * delta)));
    return std::clamp(estimate, kMinTaps, kMaxTaps);
}

FirCoefficients FirCoefficients::fromJson(const QJsonObject &object, QString *error,
                                          const QString &baseDirectory)
{
    setError(error, QString());
    FirCoefficients result;

    QString ref = object.value(QLatin1String("ref")).toString();
    const double cutoff = object.value(QLatin1String("cutoff")).toDouble(0.0);
    const double sampleRate = object.value(QLatin1String("sampleRate"))
                                  .toDouble(object.value(QLatin1String("designRate"))
                                                .toDouble(0.0));
    const double beta = object.value(QLatin1String("beta")).toDouble(0.0);
    const bool hasDelay = object.contains(QLatin1String("delay"));
    const double delay = object.value(QLatin1String("delay")).toDouble(-1.0);

    // ── Where do the taps come from? ────────────────────────────────────────
    if (object.contains(QLatin1String("taps"))) {
        const QJsonValue tapsValue = object.value(QLatin1String("taps"));
        if (!tapsValue.isArray()) {
            setError(error, QStringLiteral("'taps' must be an array of numbers, got %1")
                                .arg(tapsValue.type() == QJsonValue::String
                                         ? QStringLiteral("a string")
                                         : QStringLiteral("a non-array")));
            return FirCoefficients();
        }
        const QJsonArray array = tapsValue.toArray();
        result.m_taps.reserve(array.size());
        for (int i = 0; i < array.size(); ++i) {
            const QJsonValue value = array.at(i);
            if (!value.isDouble()) {
                setError(error, QStringLiteral("taps[%1] is not a number").arg(i));
                return FirCoefficients();
            }
            result.m_taps.append(value.toDouble());
        }
    } else if (object.contains(QLatin1String("file"))) {
        const QString file = object.value(QLatin1String("file")).toString();
        if (file.isEmpty()) {
            setError(error, QStringLiteral("'file' is empty"));
            return FirCoefficients();
        }
        QString path = file;
        if (QFileInfo(path).isRelative() && !baseDirectory.isEmpty())
            path = QDir(baseDirectory).absoluteFilePath(file);
        if (!QFileInfo::exists(path)) {
            setError(error, QStringLiteral("coefficient file not found: %1").arg(path));
            return FirCoefficients();
        }
        if (ref.isEmpty())
            ref = QFileInfo(path).fileName();
        if (path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
            QFile handle(path);
            if (!handle.open(QIODevice::ReadOnly)) {
                setError(error, QStringLiteral("cannot read %1: %2").arg(path, handle.errorString()));
                return FirCoefficients();
            }
            QJsonParseError parseError{};
            const QJsonDocument doc = QJsonDocument::fromJson(handle.readAll(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                setError(error, QStringLiteral("%1: %2").arg(path, parseError.errorString()));
                return FirCoefficients();
            }
            FirCoefficients nested = fromJson(doc.object(), error,
                                              QFileInfo(path).absolutePath());
            if (nested.isEmpty())
                return FirCoefficients();
            if (ref.isEmpty() || ref == QFileInfo(path).fileName())
                ref = nested.ref();
            result = nested;
        } else {
            FirCoefficients loaded = fromTapsFile(path, error);
            if (loaded.isEmpty())
                return FirCoefficients();
            result = loaded;
            if (!ref.isEmpty())
                result.m_ref = ref;
        }
    } else {
        setError(error, QStringLiteral("no taps: expected 'taps' or 'file'"));
        return FirCoefficients();
    }

    result.m_ref = ref.isEmpty() ? QStringLiteral("inline") : ref;
    result.m_cutoff = cutoff;
    result.m_sampleRate = sampleRate;
    result.m_beta = beta;
    result.m_delay = hasDelay ? delay : -1.0;

    if (!result.validate(error))
        return FirCoefficients();

    // A recorded sha256 is an assertion about the numbers; trusting it without
    // checking would make the reproducible-result claim decorative.
    const QString recorded = object.value(QLatin1String("sha256")).toString().toLower();
    if (!recorded.isEmpty() && recorded != result.sha256OfTaps()) {
        setError(error, QStringLiteral("sha256 mismatch: document says %1, taps hash to %2")
                            .arg(recorded, result.sha256OfTaps()));
        return FirCoefficients();
    }
    return result;
}

FirCoefficients FirCoefficients::fromTapsFile(const QString &path, QString *error)
{
    setError(error, QString());
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot read %1: %2").arg(path, file.errorString()));
        return FirCoefficients();
    }
    const QByteArray raw = file.readAll();
    const QString text = QString::fromUtf8(raw);

    // A JSON document handed to the "bare numbers" loader: same intent, so
    // accept it instead of failing on the first `{`.
    if (text.trimmed().startsWith(QLatin1Char('{'))) {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            setError(error, QStringLiteral("%1: %2").arg(path, parseError.errorString()));
            return FirCoefficients();
        }
        QString nestedError;
        FirCoefficients nested = fromJson(doc.object(), &nestedError,
                                          QFileInfo(path).absolutePath());
        if (nested.isEmpty()) {
            setError(error, QStringLiteral("%1: %2").arg(path, nestedError));
            return FirCoefficients();
        }
        return nested;
    }

    FirCoefficients result;
    const QRegularExpression separator(QStringLiteral("\\s+"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int lineNo = 0; lineNo < lines.size(); ++lineNo) {
        QString line = lines.at(lineNo);
        const int comment = line.indexOf(QLatin1Char('#'));
        if (comment >= 0)
            line.truncate(comment);
        const QStringList tokens = line.split(separator, Qt::SkipEmptyParts);
        for (const QString &token : tokens) {
            bool ok = false;
            const double value = token.toDouble(&ok);
            if (!ok || !std::isfinite(value)) {
                setError(error, QStringLiteral("%1:%2: '%3' is not a finite number")
                                    .arg(path)
                                    .arg(lineNo + 1)
                                    .arg(token));
                return FirCoefficients();
            }
            result.m_taps.append(value);
        }
    }
    if (result.m_taps.isEmpty()) {
        setError(error, QStringLiteral("%1: no taps found").arg(path));
        return FirCoefficients();
    }
    result.m_ref = QFileInfo(path).fileName();
    if (!result.validate(error))
        return FirCoefficients();
    return result;
}

QString FirCoefficients::sha256OfTaps() const
{
    const QByteArray digest = QCryptographicHash::hash(tapsAsCanonicalBytes(m_taps),
                                                       QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

double FirCoefficients::gain() const
{
    double sum = 0.0;
    for (double h : m_taps)
        sum += h;
    return sum;
}

double FirCoefficients::delaySamples() const
{
    if (m_taps.isEmpty())
        return 0.0;
    return 0.5 * double(m_taps.size() - 1);
}

bool FirCoefficients::validate(QString *error) const
{
    setError(error, QString());

    if (m_taps.isEmpty()) {
        setError(error, QStringLiteral("no coefficients loaded"));
        return false;
    }
    if (m_taps.size() < kMinTaps) {
        setError(error, QStringLiteral("too few taps: %1 (minimum %2)")
                            .arg(m_taps.size())
                            .arg(kMinTaps));
        return false;
    }
    if (m_taps.size() > kMaxTaps) {
        setError(error, QStringLiteral("too many taps: %1 (maximum %2)")
                            .arg(m_taps.size())
                            .arg(kMaxTaps));
        return false;
    }
    for (int i = 0; i < m_taps.size(); ++i) {
        if (!std::isfinite(m_taps.at(i))) {
            setError(error, QStringLiteral("taps[%1] is not finite").arg(i));
            return false;
        }
    }

    // DC gain: a low-pass with |sum − 1| > 1e-3 changes the signal level, which
    // is never what "decimate by 3" is supposed to mean.
    const double dc = gain();
    if (std::abs(dc - 1.0) > kGainTolerance) {
        setError(error, QStringLiteral("DC gain %1 is not 1 (±%2): normalise the taps")
                            .arg(dc, 0, 'g', 10)
                            .arg(kGainTolerance, 0, 'g', 3));
        return false;
    }

    // Delay consistency: a declared delay that disagrees with the linear-phase
    // centre means the document describes a different filter than it carries.
    if (m_delay >= 0.0 && std::abs(m_delay - delaySamples()) > kDelayTolerance) {
        setError(error, QStringLiteral("declared delay %1 disagrees with (taps-1)/2 = %2")
                            .arg(m_delay, 0, 'g', 10)
                            .arg(delaySamples(), 0, 'g', 10));
        return false;
    }
    return true;
}
