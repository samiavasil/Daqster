#ifndef QDEVIOSTREAMCONFIGOBSOLETE_H
#define QDEVIOSTREAMCONFIGOBSOLETE_H

#include <QString>
#include <QVector>

/**
 * @brief Metadata for a QDevIO stream, carried by GenericQDevIoConnectorObsolete.
 *
 * This struct tells the display node what type of data it is receiving
 * and how to configure the view (axes, labels, scaling).
 *
 * The connector携带 this metadata when a source node connects to a display.
 * If the source doesn't provide metadata, the display shows a config panel
 * for manual user configuration.
 *
 * @note Renamed to *_obsolete (REQ-SW-PL-023 §7) — implementation unchanged.
 */
struct QDevIOStreamConfigObsolete {
    QString type;              // "audio", "video", "sensor", "generic"
    int sampleRate = 0;        // samples per second (Hz)
    int bitsPerSample = 0;     // 8, 16, 24, 32
    int channels = 0;          // number of multiplexed channels
    bool signed_ = true;       // signed vs unsigned samples
    bool littleEndian = true;  // byte order
    double amplitudeScale = 1.0;  // value per LSB (for Y axis labeling)
    double amplitudeOffset = 0;   // DC offset
    QString unit = "V";           // "V", "mV", "raw", "dB", "normalized"
    QVector<QString> channelNames; // e.g., ["L", "R"] or ["R", "G", "B"]
};

#endif // QDEVIOSTREAMCONFIGOBSOLETE_H
