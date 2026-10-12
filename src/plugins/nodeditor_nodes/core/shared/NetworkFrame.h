#pragma once

#include "NodeDataTypes/SampledStreamDescriptor.h"

#include <QByteArray>
#include <QtEndian>

#include <cstring>

/**
 * @brief Wire framing for SampledData transport (REQ-SW-PL-044).
 *
 * ## Frame layout — version 2 (self-describing)
 *
 * All scalars are little-endian. Offsets are fixed, so the receiver always
 * knows where the raw samples begin.
 *
 *   off  size  field
 *     0    4  magic "MSSD"
 *     4    4  version (= 2)
 *     8    4  sampleCount    — number of frames in this buffer
 *    12    4  bytesPerSample — bytes per frame (== SampledStreamDescriptor::bytesPerFrame())
 *    16    8  sampleRate     — double, Hz
 *    24    4  channelCount
 *    28    1  sampleType     — SampleType, identical for every channel
 *    29    1  endianness     — SampleEndian
 *    30    1  flags          — reserved, 0
 *    31    1  reserved, 0
 *    32   ..  raw sample bytes, interleaved per SampledStreamDescriptor
 *
 * - UDP: each datagram carries exactly one complete frame.
 * - TCP: the same frame, each preceded by a 4-byte little-endian length that
 *   counts the WHOLE frame (header + payload). The receiver must handle frames
 *   split across reads and several frames arriving in one read.
 *
 * ## Why v2 carries the descriptor
 *
 * v1 deliberately omitted sampleRate / channels / sample type and expected both
 * ends to be configured identically in the UI. That cannot work in practice: the
 * sink streams whatever its upstream node produces (for a live microphone that
 * is the hardware's real rate), and nothing links the two configurations. The
 * receiving side then interpreted real bytes with an unrelated descriptor.
 *
 * The wire descriptor is now authoritative. A receiver's UI descriptor fields
 * are advisory only — they are compared against the wire for a mismatch warning
 * and never used to reinterpret the payload.
 *
 * ## Compatibility
 *
 * v1 frames are rejected, not parsed. There is nothing to stay compatible with:
 * v1 TCP never delivered a single sample (the sender wrote no length prefix at
 * all), and v1 UDP delivered the 8 header bytes as if they were audio. No peer
 * ever interoperated with either.
 *
 * v2 carries one sample type for all channels. Heterogeneous channel layouts
 * are refused by the sender rather than silently mangled; channel *names* are
 * UI cosmetics and are not transmitted (the receiver generates ch0..chN).
 */
namespace NetworkFrame {

constexpr quint32 Magic = 0x4D535344; // "MSSD"
constexpr quint32 Version = 2;

/// Size of the fixed frame header in bytes.
constexpr int HeaderSize = 32;

/// TCP length prefix in front of each frame, in bytes.
constexpr int LengthPrefixSize = 4;

/// Upper bound for one frame, used as a stream sanity check (8 MiB).
constexpr int MaxFrameSize = 8 * 1024 * 1024;

/// Everything the header carries. `bytesPerSample` is really bytes per FRAME,
/// matching SampledStreamDescriptor::bytesPerFrame().
struct Header {
    quint32 magic = 0;
    quint32 version = 0;
    quint32 sampleCount = 0;   // frames
    quint32 bytesPerSample = 0; // == bytesPerFrame()
    double sampleRate = 0.0;
    quint32 channelCount = 0;
    SampleType sampleType = SampleType::INT16;
    SampleEndian endianness = SampleEndian::LittleEndian;
    quint8 flags = 0;
};

/// True when every channel of `desc` carries the same sample type — the only
/// layout v2 can describe.
inline bool isHomogeneous(const SampledStreamDescriptor &desc)
{
    if (desc.channels.isEmpty())
        return false;
    const SampleType first = desc.channels.first().sampleType;
    for (const StreamChannelDescriptor &ch : desc.channels) {
        if (ch.sampleType != first)
            return false;
    }
    return true;
}

/// Build the wire header for `desc`. `buffer` supplies the frame count, so a
/// trailing partial frame never inflates sampleCount.
inline Header headerFor(const QByteArray &buffer, const SampledStreamDescriptor &desc)
{
    const quint32 frameBytes = static_cast<quint32>(desc.bytesPerFrame());

    Header hdr;
    hdr.magic = Magic;
    hdr.version = Version;
    hdr.sampleCount = frameBytes > 0 ? static_cast<quint32>(buffer.size() / frameBytes) : 0;
    hdr.bytesPerSample = frameBytes;
    hdr.sampleRate = desc.sampleRate;
    hdr.channelCount = static_cast<quint32>(desc.channels.size());
    hdr.sampleType = desc.channels.isEmpty() ? SampleType::INT16 : desc.channels.first().sampleType;
    hdr.endianness = desc.endianness;
    hdr.flags = 0;
    return hdr;
}

/// Encode one complete frame (header + raw bytes).
inline QByteArray encode(const QByteArray &rawBytes, const SampledStreamDescriptor &desc)
{
    const Header hdr = headerFor(rawBytes, desc);

    QByteArray frame;
    frame.reserve(HeaderSize + rawBytes.size());
    frame.append(reinterpret_cast<const char *>(&Magic), 4);
    const quint32 leVersion = qToLittleEndian(hdr.version);
    const quint32 leCount = qToLittleEndian(hdr.sampleCount);
    const quint32 leBps = qToLittleEndian(hdr.bytesPerSample);
    const quint32 leChannels = qToLittleEndian(hdr.channelCount);
    frame.append(reinterpret_cast<const char *>(&leVersion), 4);
    frame.append(reinterpret_cast<const char *>(&leCount), 4);
    frame.append(reinterpret_cast<const char *>(&leBps), 4);
    frame.append(reinterpret_cast<const char *>(const_cast<double *>(&hdr.sampleRate)), 8);
    frame.append(reinterpret_cast<const char *>(&leChannels), 4);
    frame.append(reinterpret_cast<const char *>(&hdr.sampleType), 1);
    frame.append(reinterpret_cast<const char *>(&hdr.endianness), 1);
    frame.append(reinterpret_cast<const char *>(&hdr.flags), 1);
    frame.append("\0", 1);
    frame.append(rawBytes);
    return frame;
}

/// Decode a complete frame. Returns false on a short frame, bad magic, an
/// unsupported version, or a payload that is not a whole number of frames.
/// `payload` receives the raw sample bytes exactly as they were sent.
inline bool decode(const QByteArray &frame, Header &hdr, QByteArray &payload)
{
    if (frame.size() < HeaderSize)
        return false;

    hdr = Header();
    std::memcpy(&hdr.magic, frame.constData(), 4);
    std::memcpy(&hdr.version, frame.constData() + 4, 4);
    std::memcpy(&hdr.sampleCount, frame.constData() + 8, 4);
    std::memcpy(&hdr.bytesPerSample, frame.constData() + 12, 4);
    std::memcpy(&hdr.sampleRate, frame.constData() + 16, 8);
    std::memcpy(&hdr.channelCount, frame.constData() + 24, 4);
    std::memcpy(&hdr.sampleType, frame.constData() + 28, 1);
    std::memcpy(&hdr.endianness, frame.constData() + 29, 1);
    std::memcpy(&hdr.flags, frame.constData() + 30, 1);

    hdr.magic = qFromLittleEndian(hdr.magic);
    hdr.version = qFromLittleEndian(hdr.version);
    hdr.sampleCount = qFromLittleEndian(hdr.sampleCount);
    hdr.bytesPerSample = qFromLittleEndian(hdr.bytesPerSample);
    hdr.channelCount = qFromLittleEndian(hdr.channelCount);

    if (hdr.magic != Magic)
        return false;
    if (hdr.version != Version)
        return false;
    if (hdr.channelCount == 0)
        return false;
    if (hdr.bytesPerSample == 0)
        return false;

    payload = frame.mid(HeaderSize);

    // A payload that is not a whole number of frames would shift every later
    // frame by a fraction of a sample — reject instead of decoding garbage.
    if (static_cast<quint32>(payload.size()) != hdr.sampleCount * hdr.bytesPerSample)
        return false;

    return true;
}

/// Build the 4-byte little-endian length prefix for one whole frame.
inline QByteArray lengthPrefix(const QByteArray &frame)
{
    const quint32 le = qToLittleEndian(static_cast<quint32>(frame.size()));
    return QByteArray(reinterpret_cast<const char *>(&le), LengthPrefixSize);
}

/// Rebuild a SampledStreamDescriptor from a wire header. Channel names are
/// generated (ch0..chN) — names are not part of the protocol.
inline SampledStreamDescriptor descriptorFor(const Header &hdr)
{
    SampledStreamDescriptor desc;
    desc.sampleRate = hdr.sampleRate;
    desc.endianness = hdr.endianness;
    for (quint32 i = 0; i < hdr.channelCount; ++i)
        desc.channels.append({QStringLiteral("ch%1").arg(i), hdr.sampleType});
    desc.unit = QStringLiteral("raw");
    desc.domain = QStringLiteral("network");
    return desc;
}

} // namespace NetworkFrame