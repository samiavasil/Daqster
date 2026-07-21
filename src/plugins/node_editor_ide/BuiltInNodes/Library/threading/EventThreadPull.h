#ifndef EVENTTHREADPULL_H
#define EVENTTHREADPULL_H

#include <QObject>
#include <QThread>

class EventThreadPull : public QObject
{
    Q_OBJECT
public:
    ~EventThreadPull();

    void AddWorker(QObject* worker);
    void stop();

    static EventThreadPull& instance();

public slots:
    void destroyedWorker(QObject* obj);

protected:
    EventThreadPull();

private:
    QThread m_WorkerThread;
};

#endif // EVENTTHREADPULL_H
