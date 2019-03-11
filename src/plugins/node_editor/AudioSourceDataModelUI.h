#ifndef AUDIOSOURCEDATAMODELUI_H
#define AUDIOSOURCEDATAMODELUI_H

#include <QWidget>
#include <QtMultimedia/QAudioDeviceInfo>

namespace Ui {
class AudioSourceDataModelUI;
}
class QAudioFormat;
class AudioSourceDataModelUI : public QWidget
{
    Q_OBJECT

public:
    explicit AudioSourceDataModelUI(QAudio::Mode mode = QAudio::AudioInput, QWidget *parent = 0);
    ~AudioSourceDataModelUI();


    const QAudioFormat& FormatAudio() const;

    bool isFormatSupported(const QAudioFormat &format) const;

signals:
    void ReloadAudioConnection();

protected slots:
    void InitAudioParams(int idx);
    void ChannelNumberChanged(int val);
    void CodecChanged(int val);
    void ByteOdrerChanged(int val);
    void SampleRateChanged(int val);
    void SampleSizeChanged(int val);
    void SampleTypeChanged(int val);

private:
    Ui::AudioSourceDataModelUI *ui;
    QAudioFormat     m_FormatAudio;
    QList<QAudioDeviceInfo> m_devs;
};

#endif // AUDIOSOURCEDATAMODELUI_H
