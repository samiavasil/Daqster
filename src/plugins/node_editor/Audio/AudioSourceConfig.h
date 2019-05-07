#ifndef AUDIOSOURCECONFIG_H
#define AUDIOSOURCECONFIG_H

#include <QWidget>
#include <QtMultimedia/QAudioDeviceInfo>

namespace Ui {
class AudioSourceConfig;
}

class AudioSourceConfig : public QWidget
{
    Q_OBJECT

public:
    explicit AudioSourceConfig(QAudio::Mode mode,
                               QAudioDeviceInfo &devInfo,
                               QAudioFormat &formatAudio,
                               QWidget *parent = 0);
    ~AudioSourceConfig();


    const QAudioFormat& FormatAudio() const;

    QAudioDeviceInfo DevInfo() const;

    bool isFormatSupported(const QAudioFormat &format) const;

    void show();

signals:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);

protected slots:
    void InitAudioParams(int idx);
    void ChannelNumberChanged(int val);
    void CodecChanged(int val);
    void ByteOdrerChanged(int val);
    void SampleRateChanged(int val);
    void SampleSizeChanged(int val);
    void SampleTypeChanged(int val);

private:
    Ui::AudioSourceConfig *ui;
    QList<QAudioDeviceInfo> m_Devs;
    QAudioDeviceInfo& m_DevInfo;
    QAudioFormat&     m_FormatAudio;
    QAudio::Mode m_Mode;

};

#endif // AUDIOSOURCECONFIG_H
