#ifndef EVENTTHREADPULLOBSOLETE_H
#define EVENTTHREADPULLOBSOLETE_H

#include <QObject>
#include <QThread>

#include "NodeEditorLibraryExport.h"

class NODE_EDITOR_LIBRARY_EXPORT EventThreadPullObsolete : public QObject
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
