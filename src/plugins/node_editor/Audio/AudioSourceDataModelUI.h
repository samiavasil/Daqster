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
    explicit AudioSourceDataModelUI(QAudioDeviceInfo &devInfo,
                                    QAudioFormat &formatAudio,
                                    QWidget *parent = nullptr);
    ~AudioSourceDataModelUI();
    const QAudioFormat FormatAudio() const;

    QAudioDeviceInfo DevInfo() const;

signals:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);
    void Start(AudioSourceDataModel::StartStop start);

public slots:
    void AudioStateChanged(QAudio::State state);
private slots:
    void Start(bool start);
    void ConfigAudio();
protected:
    virtual void enterEvent(QEvent *event);
private:
    Ui::AudioSourceDataModelUI *ui;
    QAudioDeviceInfo&  m_devInfo;
    QAudioFormat&      m_formatAudio;
    AudioSourceConfig m_Conf;
};

#endif // AUDIOSOURCEDATAMODELUI_H
