#ifndef QAUDIOINPUTTHREAD_H
#define QAUDIOINPUTTHREAD_H

#include <memory>
#include<QThread>
#include<QSharedPointer> //DELL MEEEE
#include <QAudioInput> //DELL
class QAudioInput; //DELL

class InEventLoopWorker : public QObject
{
    Q_OBJECT
public:
    InEventLoopWorker(QObject* parent=nullptr):QObject(parent){

    }
public slots:
    virtual void DoWork() = 0;
};

class EventThreadPull : public QObject{
    Q_OBJECT
public:

    ~EventThreadPull();
    void AddWorker(InEventLoopWorker* worker);
    void stop();
    static EventThreadPull& instance();

public slots:
    void destroyedWorker(QObject* obj);
    //    void handleResults(const QString &);
signals:
    void operate();
protected:
    EventThreadPull();
    QThread m_WorkerThread;
    static EventThreadPull m_thread_pull;
};


#endif // QAUDIOINPUTTHREAD_H
