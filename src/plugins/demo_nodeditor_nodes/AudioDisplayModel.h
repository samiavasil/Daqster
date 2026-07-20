#ifndef AUDIODISPLAYMODEL_H
#define AUDIODISPLAYMODEL_H

#include <QDevIoDisplayModel.h>

/**
 * @brief Audio-specific display model with demux from mixed streams.
 *
 * Subclass of QDevIoDisplayModel that:
 * - Auto-configures for audio streams (waveform + FFT)
 * - Demuxes audio channels from mixed QDevIO streams
 * - Sets up XYSeriesIODevice with proper audio format
 */
class AudioDisplayModel : public QDevIoDisplayModel
{
    Q_OBJECT

public:
    AudioDisplayModel();
    ~AudioDisplayModel() override;

    QString caption() const override
    { return QStringLiteral("Audio Display"); }

    QString name() const override
    { return QStringLiteral("AudioDisplay"); }

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex const portIndex) override;

private:
    void configureAudioView(int channels, int sampleRate);
};

#endif // AUDIODISPLAYMODEL_H
