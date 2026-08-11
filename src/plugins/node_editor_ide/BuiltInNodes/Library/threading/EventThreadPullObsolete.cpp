#include <EventThreadPullObsolete.h>
#include <QThread>
#include <QDebug>
#include "LogCategories.h"

EventThreadPullObsolete::EventThreadPullObsolete()
    : m_WorkerThread(this)
{
    m_WorkerThread.start();
}

EventThreadPullObsolete::~EventThreadPullObsolete()
{
    m_WorkerThread.quit();
    if (!m_WorkerThread.wait(QDeadlineTimer(1000))) {
        m_WorkerThread.terminate();
        m_WorkerThread.wait();
    }
}

void EventThreadPullObsolete::AddWorker(QObject *worker)
{
    worker->moveToThread(&m_WorkerThread);
    connect(&m_WorkerThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &QObject::destroyed, this, &EventThreadPullObsolete::destroyedWorker);
    QMetaObject::invokeMethod(worker, "DoWork", Qt::QueuedConnection);
}

void EventThreadPullObsolete::stop()
{
    m_WorkerThread.quit();
    if (!m_WorkerThread.wait(QDeadlineTimer(1000))) {
        m_WorkerThread.terminate();
        m_WorkerThread.wait();
    }
}

void EventThreadPullObsolete::destroyedWorker(QObject *obj)
{
    qCDebug(lcNodeEditor) << "Worker destroyed:" << static_cast<void*>(obj);
}

EventThreadPullObsolete& EventThreadPullObsolete::instance()
{
    static EventThreadPullObsolete s_thread_pull;
    return s_thread_pull;
}
