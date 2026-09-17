#pragma once

#include <QByteArray>
#include <QtEndian>

#include <cstring>

/**
 * @brief Length-prefixed wire framing for SampledData transport (REQ-SW-PL-044).
 *
 * Frame layout (all integers little-endian on the wire):
 *   [4-byte magic "MSSD"][4-byte sampleCount][4-byte bytesPerSample][raw bytes]
 *
 * - UDP: each datagram carries exactly one frame.
 * - TCP: the same framing is used as a byte stream; the receiver must handle
 *   partial frames (a frame may span multiple reads / multiple frames may
 *   arrive in one read).
 *
 * The descriptor (sampleRate, channels) is NOT carried on the wire — it is
 * configured in the UI on both sides (v1, no out-of-band exchange). The wire
 * carries only the raw sample bytes.
 */
namespace NetworkFrame {

constexpr quint32 Magic = 0x4D535344; // "MSSD"

struct Header {
    quint32 magic = 0;
    quint32 sampleCount = 0;
    quint32 bytesPerSample = 0;
};

/// Size of the fixed header in bytes.
constexpr int HeaderSize = 12;

/// Encode a frame from raw sample bytes + header fields.
inline QByteArray encode(const QByteArray &rawBytes,
                         quint32 sampleCount,
                         quint32 bytesPerSample)
{
    QByteArray frame;
    frame.reserve(HeaderSize + rawBytes.size());
    frame.append(reinterpret_cast<const char *>(&Magic), 4);
    const quint32 leCount = qToLittleEndian(sampleCount);
    const quint32 leBps = qToLittleEndian(bytesPerSample);
    frame.append(reinterpret_cast<const char *>(&leCount), 4);
    frame.append(reinterpret_cast<const char *>(&leBps), 4);
    frame.append(rawBytes);
    return frame;
}

/// Decode a complete frame (>= HeaderSize bytes). Returns false on bad magic
/// or a truncated header. `payload` is the raw sample bytes after the header.
inline bool decode(const QByteArray &frame, Header &hdr, QByteArray &payload)
{
    if (frame.size() < HeaderSize)
        return false;

    quint32 magic = 0;
    quint32 count = 0;
    quint32 bps = 0;
    std::memcpy(&magic, frame.constData(), 4);
    std::memcpy(&count, frame.constData() + 4, 4);
    std::memcpy(&bps, frame.constData() + 8, 4);

    hdr.magic = qFromLittleEndian(magic);
    hdr.sampleCount = qFromLittleEndian(count);
    hdr.bytesPerSample = qFromLittleEndian(bps);

    if (hdr.magic != Magic)
        return false;

    payload = frame.mid(HeaderSize);
    return true;
}

} // namespace NetworkFrame
