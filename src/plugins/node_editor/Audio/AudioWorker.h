#ifndef AUDIOWORKER_H
#define AUDIOWORKER_H

#include<EventThreadPull.h>

#include<QSharedPointer>
#include <QAudioInput>
#include <AudioSourceDataModel.h>

class QAudioInput;

class AudioWorker : public InEventLoopWorker
{
    Q_OBJECT

public:
    AudioWorker(std::shared_ptr<QIODevice> devio, QObject* parent=nullptr);
    virtual ~AudioWorker();
public slots:
    void DoWork();
    void Start(AudioSourceDataModel::StartStop status);
    void UpdateAudioDevice(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);


signals:
    void resultReady(const QString &result);
    void stateChanged(QAudio::State);
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);
private:

    std::shared_ptr<QAudioInput> m_audio_src;
    std::shared_ptr<QIODevice> m_devio;
    QAudioInput* m_audio;
};

#endif // AUDIOWORKER_H
