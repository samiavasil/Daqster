#ifndef QAUDIOINPUTTHREAD_H
#define QAUDIOINPUTTHREAD_H

#include<QAudioInput>
#include<QThread>
#include <QtMultimedia/QAudioDeviceInfo>
#include<QSharedPointer>
#include<QDebug>

class AudioWorker : public QObject
{
    Q_OBJECT

public:
    AudioWorker(QObject* parent=nullptr):QObject(parent){
        m_audio_src = nullptr;
    }
    virtual ~AudioWorker(){
        m_audio_src->stop();
    }
public slots:
    void doWork(QAudioDeviceInfo devInfo, QAudioFormat formatAudio, QSharedPointer<QIODevice> devio){
        QString result;
        m_devio = devio.data();
        if(m_audio_src) {
            m_audio_src->stop();
            disconnect(m_audio_src);
            m_audio_src->deleteLater();
        }
        m_audio_src  = new QAudioInput(devInfo, formatAudio);
 //      m_audio_src->setNotifyInterval(40);
        m_audio_src->setBufferSize(1000);
        qDebug() << m_audio_src->bufferSize();
        m_audio_src->setObjectName(QString("AudioInput: %1").arg(devInfo.deviceName()));
  //      connect(m_audio_src.get(),SIGNAL(destroyed(QObject*)), this,SLOT(destroyedObj(QObject*)));
        connect(m_audio_src,SIGNAL(stateChanged(QAudio::State)), this, SIGNAL(stateChanged(QAudio::State)) );
        m_audio_src->start(m_devio);
        emit resultReady(result);
    }

signals:
    void resultReady(const QString &result);
    void stateChanged(QAudio::State);
private:
    QIODevice*    m_devio;
    QAudioInput* m_audio_src;
};

class QAudioInputThread : public QObject{
    Q_OBJECT
public:
    QAudioInputThread() {
        qRegisterMetaType<QSharedPointer<QIODevice>>("QSharedPointer<QIODevice>");
        workerThread.start();
    }
    ~QAudioInputThread() {
        workerThread.quit();
        workerThread.wait();
    }

    void start(QAudioDeviceInfo devInfo, QAudioFormat formatAudio, QSharedPointer<QIODevice> devio){

        AudioWorker *worker = new AudioWorker;
        worker->moveToThread(&workerThread);
      //  connect(worker, SIGNAL(stateChanged(QAudio::State)), this, SIGNAL(stateChanged(QAudio::State)));
        connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);
        connect(this, &QAudioInputThread::operate, worker, &AudioWorker::doWork);
//        connect(worker, &AudioWorker::resultReady, this, &QAudioInputThread::handleResults);

        emit operate(devInfo, formatAudio, devio);
    }
    void stop() {

    }
public slots:
//    void handleResults(const QString &);
signals:
    void operate(QAudioDeviceInfo devInfo, QAudioFormat formatAudio, QSharedPointer<QIODevice> devio);
    void stateChanged(QAudio::State);
protected:
    QAudioInput m_Audio;
    QThread workerThread;
};


#endif // QAUDIOINPUTTHREAD_H
