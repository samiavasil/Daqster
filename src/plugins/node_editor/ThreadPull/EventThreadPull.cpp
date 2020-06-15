#include <EventThreadPull.h>
#include<QThread>
//#include <QtMultimedia/QAudioDeviceInfo>
#include <QDebug>

EventThreadPull EventThreadPull::m_thread_pull;
EventThreadPull::EventThreadPull() {

    m_WorkerThread = new QThread(this);
    qRegisterMetaType<std::shared_ptr<QIODevice>>("std::shared_ptr<QIODevice>");
    m_WorkerThread->start();

}

EventThreadPull::~EventThreadPull() {

    m_WorkerThread->quit();
    m_WorkerThread->wait();
    m_WorkerThread->deleteLater();

}

void EventThreadPull::AddWorker(InEventLoopWorker *worker) {
    worker->moveToThread(m_WorkerThread);
    //  connect(worker, SIGNAL(stateChanged(QAudio::State)), this, SIGNAL(stateChanged(QAudio::State)));
    connect(m_WorkerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, SIGNAL(operate()), worker, SLOT(DoWork()));
    connect(worker, SIGNAL(destroyed(QObject*)), this, SLOT(destroyedWorker(QObject*)));
    emit operate();
}

void EventThreadPull::stop() {

}

void EventThreadPull::destroyedWorker(QObject *obj){
    qDebug() << "Destroy wirker object: " << static_cast<void*>(obj);
}

EventThreadPull& EventThreadPull::thread_pull()
{
    return m_thread_pull;
}
