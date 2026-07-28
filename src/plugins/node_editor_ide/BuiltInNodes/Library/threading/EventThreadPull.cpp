#include <EventThreadPull.h>
#include <QThread>
#include <QDebug>
#include "LogCategories.h"

EventThreadPull::EventThreadPull()
    : m_WorkerThread(this)
{
    m_WorkerThread.start();
}

EventThreadPull::~EventThreadPull()
{
    m_WorkerThread.quit();
    if (!m_WorkerThread.wait(QDeadlineTimer(1000))) {
        m_WorkerThread.terminate();
        m_WorkerThread.wait();
    }
}

void EventThreadPull::AddWorker(QObject *worker)
{
    worker->moveToThread(&m_WorkerThread);
    connect(&m_WorkerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &QObject::destroyed, this, &EventThreadPull::destroyedWorker);
    QMetaObject::invokeMethod(worker, "DoWork", Qt::QueuedConnection);
}

void EventThreadPull::stop()
{
    m_WorkerThread.quit();
    if (!m_WorkerThread.wait(QDeadlineTimer(1000))) {
        m_WorkerThread.terminate();
        m_WorkerThread.wait();
    }
}

void EventThreadPull::destroyedWorker(QObject *obj)
{
    qCDebug(lcNodeEditor) << "Worker destroyed:" << static_cast<void*>(obj);
}

EventThreadPull& EventThreadPull::instance()
{
    static EventThreadPull s_thread_pull;
    return s_thread_pull;
}
