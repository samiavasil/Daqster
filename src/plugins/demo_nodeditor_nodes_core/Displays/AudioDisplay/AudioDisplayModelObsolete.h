#ifndef AUDIODISPLAYMODELOBSOLETE_H
#define AUDIODISPLAYMODELOBSOLETE_H

#include <QDevIoDisplayModelObsolete.h>

/**
 * @brief Audio-specific display model with demux from mixed streams.
 *
 * Subclass of QDevIoDisplayModelObsolete that:
 * - Auto-configures for audio streams (waveform + FFT)
 * - Demuxes audio channels from mixed QDevIO streams
 * - Sets up XYSeriesIODeviceObsolete with proper audio format
 *
 * @note Renamed to *_obsolete (REQ-SW-PL-023 §7) — implementation unchanged.
 *       Kept working until the QDevIO display world is deleted at the very end.
 *       Registered under its new name AND aliased under the old "AudioDisplay"
 *       key so old saved graphs still load.
 */
class AudioDisplayModelObsolete : public QDevIoDisplayModelObsolete
{
    Q_OBJECT

public:
    AudioDisplayModelObsolete();
    ~AudioDisplayModelObsolete() override;

    QString caption() const override
    { return QStringLiteral("Audio Display (obsolete)"); }

    QString name() const override
    { return QStringLiteral("AudioDisplayObsolete"); }

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex const portIndex) override;

private:
    void configureAudioView(int channels, int sampleRate);
};

#endif // AUDIODISPLAYMODELOBSOLETE_H
