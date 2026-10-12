#ifndef EVENTTHREADPULLOBSOLETE_H
#define EVENTTHREADPULLOBSOLETE_H

#include <QObject>
#include <QThread>

class EventThreadPullObsolete : public QObject
{
    Q_OBJECT
public:
    ~EventThreadPullObsolete();

    void AddWorker(QObject* worker);
    void stop();

    static EventThreadPullObsolete& instance();

public slots:
    void destroyedWorker(QObject* obj);

protected:
    EventThreadPullObsolete();

private:
    QThread m_WorkerThread;
};

#endif // EVENTTHREADPULLOBSOLETE_H
