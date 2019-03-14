#ifndef AUDIOSOURCEDATAMODELUI_H
#define AUDIOSOURCEDATAMODELUI_H

#include <QWidget>
#include <QtMultimedia/QAudioDeviceInfo>
#include "AudioSourceConfig.h"
#include "AudioSourceDataModel.h"

namespace Ui {
class AudioSourceDataModelUI;
}

class AudioSourceDataModelUI : public QWidget
{
    Q_OBJECT

public:
    explicit AudioSourceDataModelUI(QAudioDeviceInfo &devInfo, QAudioFormat &formatAudio, QWidget *parent = 0);
    ~AudioSourceDataModelUI();
    const QAudioFormat FormatAudio() const;

    QAudioDeviceInfo DevInfo() const;

signals:
    void ChangeAudioConnection(QAudioDeviceInfo& devInfo, QAudioFormat& formatAudio);
    void Start(AudioSourceDataModel::StartStop start);

public slots:
    void AudioStateChanged(QAudio::State state);
private slots:
    void Start(bool start);
    void ConfigAudio();
private:
    Ui::AudioSourceDataModelUI *ui;
    AudioSourceConfig m_Conf;
    QAudioDeviceInfo & m_devInfo;
    QAudioFormat &  m_formatAudio;

};

#endif // AUDIOSOURCEDATAMODELUI_H
