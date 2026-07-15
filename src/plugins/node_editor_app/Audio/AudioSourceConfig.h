#ifndef AUDIOSOURCECONFIG_H
#define AUDIOSOURCECONFIG_H

#include "AudioCompat.h"

#include <QWidget>
#include "AudioComboModel.h"

namespace Ui {
class AudioSourceConfig;
}

class AudioSourceConfig : public QWidget
{
    Q_OBJECT

public:
    explicit AudioSourceConfig(AudioCompat::Mode mode,
                               QAudioDeviceInfo &devInfo,
                               QAudioFormat &formatAudio,
                               QWidget *parent = 0);
    ~AudioSourceConfig();


    const QAudioFormat& FormatAudio() const;

    bool isFormatSupported(const QAudioFormat &format) const;

    void show();

signals:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);

protected slots:
    void ABEInitAudioParams(int idx);
    void ChannelNumberChanged(int val);
    void CodecChanged(int val);
    void ByteOdrerChanged(int val);
    void SampleRateChanged(int val);
    void SampleSizeChanged(int val);
    void SampleTypeChanged(int val);

private:
    Ui::AudioSourceConfig *ui;
    QList<QAudioDeviceInfo> m_Devs;
    QAudioDeviceInfo* m_DevInfo    = nullptr;   // non-owning: externally managed
    QAudioFormat*     m_FormatAudio = nullptr;  // non-owning: externally managed
    AudioCompat::Mode m_Mode;

    // One model per combo-box; owned by this widget (parent = this)
    QAudioComboModel* m_ChannelModel    = nullptr;
    QAudioComboModel* m_CodecModel      = nullptr;
    QAudioComboModel* m_ByteOrderModel  = nullptr;
    QAudioComboModel* m_SampleRateModel = nullptr;
    QAudioComboModel* m_SampleSizeModel = nullptr;
    QAudioComboModel* m_SampleTypeModel = nullptr;
};

#endif // AUDIOSOURCECONFIG_H
