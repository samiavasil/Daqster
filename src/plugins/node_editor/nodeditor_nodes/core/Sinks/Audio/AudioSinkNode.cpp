#include "AudioSinkNode.h"

#include "LogCategories.h"

#include "NodeDataTypes/SampledData.h"

#include <QFile>
#include <QJsonValue>
#include <QVector>
#include <QtEndian>

#include <cmath>
#include <cstring>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

// ── Config contract (ported from the private `JsonConfigHelpers.h`) ──────
//
// The private plugin kept one header for all of its nodes; the public node
// needs the same four rules and nothing else, so they live here. Behaviour is
// identical, key by key:
//   * missing key        → the current value (nothing is reset);
//   * wrong JSON type    → the current value (a string where a number belongs
//                          and vice versa);
//   * out-of-range value → the current value;
// so `load()` on a partial or damaged JSON leaves the previous config intact
// instead of throwing (REQ-SW-PL-077 AC2).

/// Read a string and trim it; a path written on another machine often carries
/// spaces. A missing or non-string key yields @p current.
QString readTrimmedString(const QJsonObject &object, const QString &key,
                          const QString &current)
{
    const QJsonValue value = object.value(key);
    if (!value.isString())
        return current;
    return value.toString().trimmed();
}

/// Read a bool. A missing or non-bool key yields @p current (1/0 is not a bool).
bool readBool(const QJsonObject &object, const QString &key, bool current)
{
    const QJsonValue value = object.value(key);
    if (!value.isBool())
        return current;
    return value.toBool();
}

/// Read a number inside [lo, hi]. Missing/non-numeric/out-of-range → @p current.
double readBoundedDouble(const QJsonObject &object, const QString &key,
                         double current, double lo, double hi)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
        return current;

    const double number = value.toDouble();
    if (number < lo || number > hi)
        return current;
    return number;
}

/// One `${...}` expression (including the braces) → its expansion.
QString expandBraced(const QString &expression);

/**
 * @brief Expand `${VAR}`, `${VAR:-default}` and `${VAR:?message}` from the
 *        environment.
 *
 * Port of the private `JsonConfig::expandEnv()` so a `.flow` stays portable:
 * it carries `${DAQSTER_DEPS}/out.wav`, not an absolute path from the machine
 * that saved it. Expansion happens in `load()`, never in `save()` — a saved
 * scene describes what the node actually holds.
 *
 * An unknown variable without a default stays **literal** on purpose: a broken
 * path has to be visible in the GUI (and in `configSchema()`), not silently
 * become an empty string the node reads as "no recording". A variable that is
 * set but empty is treated as unset for the same reason. `$$` is a literal `$`.
 * There is no recursion and no `$VAR` without braces — neither occurs in the
 * flow files, and every extra rule is one more way for a path to break quietly.
 *
 * @param value JSON string (or an already expanded path — idempotent for any
 *              value that does not contain `${`)
 * @return The value with variables expanded; never throws
 */
QString expandEnv(const QString &value)
{
    if (!value.contains(QLatin1Char('$')))
        return value;   // fast path: an ordinary path changes nothing

    QString out;
    out.reserve(value.size());

    const int size = value.size();
    for (int i = 0; i < size; ) {
        if (value.at(i) != QLatin1Char('$')) {
            out.append(value.at(i));
            ++i;
            continue;
        }

        // `$$` → `$`, the only way to write a literal dollar sign.
        if (i + 1 < size && value.at(i + 1) == QLatin1Char('$')) {
            out.append(QLatin1Char('$'));
            i += 2;
            continue;
        }

        // `${...}`; without a closing brace the rest is copied unchanged.
        if (i + 1 < size && value.at(i + 1) == QLatin1Char('{')) {
            const int close = value.indexOf(QLatin1Char('}'), i + 2);
            if (close > i) {
                out.append(expandBraced(value.mid(i, close - i + 1)));
                i = close + 1;
                continue;
            }
        }

        out.append(value.at(i));
        ++i;
    }

    return out;
}

/// One `${...}` expression (including the braces) → its expansion.
QString expandBraced(const QString &expression)
{
    const QString body = expression.mid(2, expression.size() - 3);
    const int defaultSep = body.indexOf(QStringLiteral(":-"));
    const int requiredSep = body.indexOf(QStringLiteral(":?"));
    // `${X:?msg}` has no fallback: either the value or the whole literal. It is
    // checked before `:-`; two separators in one expression are invalid, but
    // the first one wins.
    const bool required = requiredSep >= 0 && (defaultSep < 0 || requiredSep < defaultSep);
    const int separator = required ? -1 : defaultSep;
    const QString name = (separator >= 0 ? body.left(separator)
                                         : required ? body.left(requiredSep)
                                                    : body).trimmed();
    const QString fallback = separator >= 0 ? body.mid(separator + 2) : QString();

    const bool isSet = !name.isEmpty()
                       && qEnvironmentVariableIsSet(name.toUtf8().constData());
    const QString current = isSet ? qEnvironmentVariable(name.toUtf8().constData())
                                  : QString();

    if (isSet && !current.isEmpty())
        return current;
    if (separator >= 0)
        return fallback;             // `${X:-default}` for a missing/empty X
    return expression;               // unknown → literal, visible in the GUI
}

// ── WAV constants ────────────────────────────────────────────────────────
constexpr int kHeaderBytes = 44;     ///< canonical RIFF/fmt/data header
constexpr int kBitsPerSample = 16;
constexpr int kChunkSizeOffset = 4;  ///< RIFF.ChunkSize field offset
constexpr int kDataSizeOffset = 40;  ///< data.Subchunk2Size field offset
constexpr double kVolumeMin = 0.0;
constexpr double kVolumeMax = 1.0;
constexpr double kDefaultRate = 24000.0;   ///< TTS/microphone default

/// Milliseconds of @p frames at @p rate (the private `AudioPcm::durationMs`).
double durationMs(int frames, double rate)
{
    if (rate <= 0.0)
        return 0.0;
    return double(frames) * 1000.0 / rate;
}

/**
 * @brief Mix a block down to mono, normalized to [-1, 1].
 *
 * `SampledData::decodeToNormalizedF32()` handles every sample type, bit depth
 * and endianness; this only averages the channels, because a WAV file with one
 * header cannot describe a channel layout that changes per block.
 *
 * @param data Input block
 * @return Mono samples; empty when the block carries no decodable frames
 */
QVector<float> toMono(const SampledData &data)
{
    QVector<QVector<float>> channels;
    data.decodeToNormalizedF32(channels);
    if (channels.isEmpty() || channels.constFirst().isEmpty())
        return {};

    const int frames = channels.constFirst().size();
    QVector<float> mono(frames, 0.0F);
    for (const QVector<float> &channel : channels) {
        const int count = qMin(frames, channel.size());
        for (int i = 0; i < count; ++i)
            mono[i] += channel.at(i);
    }

    const float scale = 1.0F / float(channels.size());
    for (float &sample : mono)
        sample *= scale;
    return mono;
}

/**
 * @brief Encode normalized float samples as little-endian 16-bit PCM.
 *
 * @param samples Mono samples in [-1, 1] (clamped here)
 * @return @c 2 * samples.size() bytes, ready for a WAV data chunk
 */
QByteArray encodePcm16(const QVector<float> &samples)
{
    QByteArray pcm16;
    pcm16.resize(samples.size() * int(sizeof(qint16)));

    char *out = pcm16.data();
    for (int i = 0; i < samples.size(); ++i) {
        const float value = qBound(-1.0F, samples.at(i), 1.0F);
        const qint16 sample = qint16(std::lround(double(value) * 32767.0));
        qToLittleEndian<qint16>(sample, out + i * int(sizeof(qint16)));
    }
    return pcm16;
}

} // namespace

AudioSinkNode::AudioSinkNode()
{
    // Deprecated playback path: one engine for the life of the node, disabled
    // until `playbackEnabled` is set (a recording never opens a device).
    m_playback.setConfig(QString(), m_config.volume, 200, m_config.playbackEnabled);
}

AudioSinkNode::~AudioSinkNode()
{
    // Single shutdown path: finalizes the WAV and closes the device.
    stop();
}

void AudioSinkNode::setConfig(const Config &config)
{
    m_config = config;
    m_config.volume = qBound(kVolumeMin, m_config.volume, kVolumeMax);

    m_playback.setConfig(QString(), m_config.volume, 200, m_config.playbackEnabled);
    if (!m_config.playbackEnabled)
        m_playback.stop();

    Q_EMIT configChanged();
}

void AudioSinkNode::setOutputPath(const QString &path)
{
    m_config.outputPath = path.trimmed();
    Q_EMIT configChanged();
}

void AudioSinkNode::setPlaybackEnabled(bool on)
{
    m_config.playbackEnabled = on;
    m_playback.setConfig(QString(), m_config.volume, 200, on);
    if (!on)
        m_playback.stop();
    Q_EMIT configChanged();
}

void AudioSinkNode::setVolume(qreal volume)
{
    m_config.volume = qBound(kVolumeMin, double(volume), kVolumeMax);
    m_playback.setConfig(QString(), m_config.volume, 200, m_config.playbackEnabled);
    Q_EMIT configChanged();
}

void AudioSinkNode::start()
{
    // Recording needs no arming — it starts with the first block. Only the
    // deprecated playback delegation has to be told the graph is running.
    if (m_config.playbackEnabled)
        m_playback.start();
}

void AudioSinkNode::stop()
{
    finalizeRecording();
    m_playback.stop();
}

unsigned int AudioSinkNode::nPorts(PortType portType) const
{
    // Sink: one input ("sample"), no output.
    return portType == PortType::In ? 1u : 0u;
}

NodeDataType AudioSinkNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    // Derived, never hardcoded: the same type every audio node emits.
    return SampledData().type();
}

std::shared_ptr<NodeData> AudioSinkNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return nullptr;
}

bool AudioSinkNode::openRecording(const QString &path, quint32 rate)
{
    finalizeRecording();

    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = QStringLiteral("cannot open %1 for writing").arg(path);
        qCWarning(lcDemoNodes).noquote() << "AudioSinkNode:" << m_error;
        return false;
    }

    // Canonical 44-byte header with zero sizes — patched as data arrives, so a
    // reader that opens the file mid-recording still sees a valid WAV.
    const quint16 channels = 1;
    const quint32 byteRate = rate * channels * kBitsPerSample / 8;
    const quint16 blockAlign = quint16(channels * kBitsPerSample / 8);

    QByteArray header(kHeaderBytes, '\0');
    char *out = header.data();
    std::memcpy(out, "RIFF", 4);
    qToLittleEndian<quint32>(quint32(36), out + kChunkSizeOffset);   // patched later
    std::memcpy(out + 8, "WAVE", 4);
    std::memcpy(out + 12, "fmt ", 4);
    qToLittleEndian<quint32>(quint32(16), out + 16);
    qToLittleEndian<quint16>(quint16(1), out + 20);                  // PCM
    qToLittleEndian<quint16>(channels, out + 22);
    qToLittleEndian<quint32>(rate, out + 24);
    qToLittleEndian<quint32>(byteRate, out + 28);
    qToLittleEndian<quint16>(blockAlign, out + 32);
    qToLittleEndian<quint16>(quint16(kBitsPerSample), out + 34);
    std::memcpy(out + 36, "data", 4);
    qToLittleEndian<quint32>(quint32(0), out + kDataSizeOffset);     // patched later

    file->write(header);

    m_file = std::move(file);
    m_fileRate = rate;
    m_dataBytes = 0;
    m_lastWritten = path;
    m_error.clear();
    return true;
}

void AudioSinkNode::appendFrames(const QByteArray &pcm16)
{
    if (!m_file)
        return;

    m_file->write(pcm16);
    m_dataBytes += quint32(pcm16.size());
    patchHeader();
}

void AudioSinkNode::patchHeader()
{
    if (!m_file)
        return;

    // RIFF.ChunkSize counts everything after this field: 36 + data bytes.
    // data.Subchunk2Size is the data chunk alone.
    char chunkSize[4];
    char dataSize[4];
    qToLittleEndian<quint32>(quint32(36) + m_dataBytes, chunkSize);
    qToLittleEndian<quint32>(m_dataBytes, dataSize);

    m_file->seek(kChunkSizeOffset);
    m_file->write(chunkSize, 4);

    m_file->seek(kDataSizeOffset);
    m_file->write(dataSize, 4);

    // Leave the cursor at the end so the next append continues the chunk.
    m_file->seek(m_file->size());
}

void AudioSinkNode::finalizeRecording()
{
    if (!m_file)
        return;

    patchHeader();
    m_file->close();
    m_file.reset();
    m_fileRate = 0;
}

void AudioSinkNode::play(const SampledData &data)
{
    if (!m_config.playbackEnabled)
        return;

    if (data.buffer().isEmpty())
        return;

    // A block is the only thing that opens the device: a graph that is merely
    // being edited (load(), setConfig()) must not grab the output device, the
    // same rule AudioOutputNode follows. Starting here also covers "playback was
    // enabled after the graph started" without arming a device nobody uses.
    if (!m_playback.isRunning())
        m_playback.start();

    // The engine converts to the device format and drops blocks on backpressure
    // — never a sink per block.
    m_playback.push(data);
}

void AudioSinkNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    if (portIndex != 0)
        return;

    const auto sampled = std::dynamic_pointer_cast<SampledData>(data);
    if (!sampled || sampled->buffer().isEmpty())
        return;   // nullptr / empty block: nothing to write, the file survives

    const QVector<float> mono = toMono(*sampled);
    if (mono.isEmpty())
        return;

    const double rate = sampled->sampleRate() > 0.0 ? sampled->sampleRate() : kDefaultRate;

    play(*sampled);

    ++m_blocks;
    m_totalMs += durationMs(mono.size(), rate);

    if (m_config.outputPath.isEmpty()) {
        // No recording requested — not an error, just a playback-only node.
        m_error.clear();
        return;
    }

    const quint32 roundedRate = quint32(std::lround(rate));
    if (!m_file || m_fileRate != roundedRate) {
        // First block, or a stream whose rate changed: a WAV header cannot
        // describe both, so the recording restarts instead of lying about the
        // samples already on disk.
        if (!openRecording(m_config.outputPath, roundedRate))
            return;
    }

    appendFrames(encodePcm16(mono));

    qCInfo(lcDemoNodes).noquote()
        << QStringLiteral("AudioSinkNode: block #%1 appended, %2 ms at %3 Hz → %4 (%5 bytes)")
               .arg(m_blocks)
               .arg(durationMs(mono.size(), rate), 0, 'f', 1)
               .arg(roundedRate)
               .arg(m_lastWritten)
               .arg(m_dataBytes);
}

QJsonObject AudioSinkNode::save() const
{
    // Base save() adds "model-name" = name() (REQ-SW-PL-057) — the
    // registration key in a .flow file.
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    modelJson[QStringLiteral("outputPath")] = m_config.outputPath;
    modelJson[QStringLiteral("playbackEnabled")] = m_config.playbackEnabled;
    modelJson[QStringLiteral("volume")] = m_config.volume;
    return modelJson;
}

void AudioSinkNode::load(QJsonObject const &p)
{
    Config config = m_config;

    // Paths are trimmed (a .flow written on another machine often carries
    // spaces) and expanded from the environment — here and only here, so a
    // saved scene holds the real path.
    config.outputPath = expandEnv(readTrimmedString(p, QStringLiteral("outputPath"),
                                                   config.outputPath));
    config.playbackEnabled = readBool(p, QStringLiteral("playbackEnabled"),
                                      config.playbackEnabled);
    config.volume = readBoundedDouble(p, QStringLiteral("volume"), config.volume,
                                      kVolumeMin, kVolumeMax);

    // setConfig() clamps too: a hand-edited file must not smuggle an
    // out-of-range volume past the invariant and into save().
    setConfig(config);
}

QJsonObject AudioSinkNode::configSchema() const
{
    // Describes exactly the object save() emits (AC2). "model-name" is part of
    // that object (written by the base save(), REQ-SW-PL-057) and is read-only.
    QJsonObject properties;

    properties.insert(QStringLiteral("outputPath"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("default"), QString()},
        {QStringLiteral("description"),
         QStringLiteral("WAV output path (empty = no recording). "
                        "${VAR} is expanded from the environment on load()")}});

    properties.insert(QStringLiteral("playbackEnabled"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("boolean")},
        {QStringLiteral("default"), false},
        {QStringLiteral("deprecated"), true},
        {QStringLiteral("description"),
         QStringLiteral("Deprecated — use a separate AudioOutput node. Kept for "
                        ".flow compatibility: plays the incoming audio through "
                        "one persistent device")}});

    properties.insert(QStringLiteral("volume"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("number")},
        {QStringLiteral("minimum"), kVolumeMin},
        {QStringLiteral("maximum"), kVolumeMax},
        {QStringLiteral("default"), 1.0},
        {QStringLiteral("description"), QStringLiteral("Playback volume, 0..1")}});

    properties.insert(QStringLiteral("model-name"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("const"), QStringLiteral("AudioSinkNode")},
        {QStringLiteral("readOnly"), true},
        {QStringLiteral("description"), QStringLiteral("Node type (REQ-SW-PL-057)")}});

    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), properties},
        {QStringLiteral("additionalProperties"), false}};
}
