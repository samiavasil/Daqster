#include "QProcessManager.h"
#include "debug.h"
#include <QDebug>
#include <QVariant>

namespace Daqster {

QProcessManager::QProcessManager(QObject *parent)
    : QObject(parent)
    , m_nextHandle(0)
{
}

QProcessManager::~QProcessManager()
{
    DEBUG << "QProcessManager destructor: invoking KillAll()";
    KillAll();
}

bool QProcessManager::GetProcessDescriptor(const ProcessHandle_t& handle, 
                                          ProcessDescriptor_t& desc) const
{
    if (m_processDescriptors.contains(handle)) {
        desc = m_processDescriptors[handle];
        return true;
    }
    return false;
}

void QProcessManager::StartProcess(const QString& name, 
                                  const QStringList& arguments, 
                                  QProcess::OpenMode mode)
{
    QProcess *newProc = new QProcess(this);
    if (newProc) {
        ProcessDescriptor_t desc = {name, arguments, mode};
        DEBUG << "QProcessManager: Starting process:" << name << "Args:" << arguments;
        
        newProc->setInputChannelMode(QProcess::ManagedInputChannel);
        
        // Allow subclass to customize environment
        setupProcessEnvironment(newProc, name, arguments);
        
        // Start the process
        newProc->start(name, arguments, mode);
        
        // Store process info
        m_processMap[m_nextHandle] = newProc;
        m_processDescriptors[m_nextHandle] = desc;
        newProc->setProperty("ProcessHandle", QVariant::fromValue(static_cast<unsigned int>(m_nextHandle)));
        
        // Connect finished signal
        connect(newProc, SIGNAL(finished(int, QProcess::ExitStatus)), 
                this, SLOT(OnProcessFinished(int, QProcess::ExitStatus)));
        
        DEBUG << "QProcessManager: Process started with handle:" << m_nextHandle;
        if (!newProc->waitForStarted(5000)) {
            qWarning() << "QProcessManager: Process failed to start within 5s:" << name;
        }

        emit ProcessEvent(m_nextHandle, PROCESS_STARTED);
        m_nextHandle++;
    } else {
        qWarning() << "QProcessManager: Failed to create QProcess for:" << name;
    }
}

void QProcessManager::KillAll()
{
    DEBUG << "QProcessManager::KillAll() called";
    
    // Snapshot the keys so we can iterate safely while OnProcessFinished modifies the map
    QList<ProcessHandle_t> handles = m_processMap.keys();
    
    for (const ProcessHandle_t& handle : handles) {
        if (m_processMap.contains(handle)) {
            QProcess* process = m_processMap[handle];
            if (nullptr != process) {
                DEBUG << "QProcessManager: Requesting graceful shutdown for:"
                         << process->program();
                process->terminate();
                
                if (process->waitForFinished(10000)) {
                    DEBUG << "QProcessManager: Process stopped gracefully";
                } else {
                    qWarning() << "QProcessManager: Process did not respond, forcing kill:"
                              << process->program();
                    process->kill();
                    process->waitForFinished(1000);
                }
            }
        }
    }
}

void QProcessManager::Kill(const ProcessHandle_t& handle)
{
    if (!m_processMap.contains(handle)) {
        qWarning() << "QProcessManager::Kill(): unknown handle:" << handle;
        return;
    }

    QProcess* process = m_processMap[handle];
    DEBUG << "QProcessManager::Kill() handle:" << handle << "process:" << process->program();

    process->terminate();
    if (process->waitForFinished(10000)) {
        DEBUG << "QProcessManager: Process" << handle << "stopped gracefully";
    } else {
        qWarning() << "QProcessManager: Process" << handle << "did not respond, forcing kill";
        process->kill();
        process->waitForFinished(1000);
    }
}

void QProcessManager::OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *proc = dynamic_cast<QProcess*>(QObject::sender());
    if (proc) {
        ProcessHandle_t handle = static_cast<ProcessHandle_t>(proc->property("ProcessHandle").toUInt());
        
        if (m_processMap.contains(handle)) {
            m_processMap[handle]->deleteLater();
            m_processMap.remove(handle);
            m_processDescriptors.remove(handle);
            
            DEBUG << "QProcessManager: Process stopped. Handle:" << handle 
                     << "ExitCode:" << exitCode 
                     << "ExitStatus:" << exitStatus;

            // Emit AFTER cleanup so slots don't see stale data
            emit ProcessEvent(handle, PROCESS_STOPPED);
        } else {
            qWarning() << "QProcessManager: Can't find process with handle:" << handle;
        }
    }
    
    // Check if all processes finished
    if (m_processMap.isEmpty()) {
        DEBUG << "QProcessManager: All processes finished";
        onAllProcessesFinished();
    }
}

void QProcessManager::setupProcessEnvironment(QProcess* process, 
                                             const QString& name,
                                             const QStringList& arguments)
{
    Q_UNUSED(process);
    Q_UNUSED(name);
    Q_UNUSED(arguments);
    // Default implementation does nothing
    // Subclasses should override to set custom environment
}

void QProcessManager::onAllProcessesFinished()
{
    // Default implementation does nothing
    // Subclasses can override to quit app, etc.
}

} // namespace Daqster
