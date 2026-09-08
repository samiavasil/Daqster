#ifndef AUDIOWORKEROBSOLETE_H
#define AUDIOWORKEROBSOLETE_H

#include "AudioCompat.h"
#include "AudioSourceDataModelUI.h"

#include<EventThreadPullObsolete.h>

#include<QSharedPointer>

class AudioWorkerObsolete : public QObject
{
    Q_OBJECT

public:
    AudioWorkerObsolete(std::shared_ptr<QIODevice> devio, QObject* parent=nullptr);
    virtual ~AudioWorkerObsolete();
public slots:
    void DoWork();
    void Start(AudioSourceDataModelUI::StartStop status);
    void UpdateAudioDevice(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);


signals:
    void resultReady(const QString &result);
    void stateChanged(QAudio::State);
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);
private:

    std::shared_ptr<AudioCompat::AudioInput> m_audio_src;
    std::shared_ptr<QIODevice> m_devio;
    AudioCompat::AudioInput* m_audio;
};

#endif // AUDIOWORKEROBSOLETE_H
