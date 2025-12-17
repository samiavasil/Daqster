#include "QProcessManager.h"
#include <QDebug>

namespace Daqster {

QProcessManager::QProcessManager(QObject *parent)
    : QObject(parent)
    , m_nextHandle(0)
{
}

QProcessManager::~QProcessManager()
{
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
        qDebug() << "QProcessManager: Starting process:" << name << "Args:" << arguments;
        
        newProc->setInputChannelMode(QProcess::ManagedInputChannel);
        
        // Allow subclass to customize environment
        setupProcessEnvironment(newProc, name, arguments);
        
        // Start the process
        newProc->start(name, arguments, mode);
        
        // Store process info
        m_processMap[m_nextHandle] = newProc;
        m_processDescriptors[m_nextHandle] = desc;
        newProc->setProperty("ProcessHandle", m_nextHandle);
        
        // Connect finished signal
        connect(newProc, SIGNAL(finished(int, QProcess::ExitStatus)), 
                this, SLOT(OnProcessFinished(int, QProcess::ExitStatus)));
        
        qDebug() << "QProcessManager: Process started with handle:" << m_nextHandle;
        newProc->waitForStarted();

        emit ProcessEvent(m_nextHandle, PROCESS_STARTED);
        m_nextHandle++;
    } else {
        qWarning() << "QProcessManager: Failed to create QProcess for:" << name;
    }
}

void QProcessManager::KillAll()
{
    blockSignals(true);
    
    // Create a copy of process list to avoid iterator invalidation
    // when OnProcessFinished() is called during waitForFinished()
    QList<QProcess*> processList = m_processMap.values();
    
    for (QProcess* process : processList) {
        if (nullptr != process) {
            // Try graceful shutdown first
            qDebug() << "QProcessManager: Requesting graceful shutdown for:" 
                     << process->program();
            process->terminate(); // Sends SIGTERM on Unix, WM_CLOSE on Windows
            
            if (process->waitForFinished(10000)) {
                qDebug() << "QProcessManager: Process stopped gracefully";
            } else {
                qWarning() << "QProcessManager: Process did not respond, forcing kill:" 
                          << process->program();
                process->kill(); // Force kill with SIGKILL
                process->waitForFinished(1000);
            }
        }
    }
    
    blockSignals(false);
    // Note: OnProcessFinished() will clean up m_processMap and m_processDescriptors
}

void QProcessManager::OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    blockSignals(true);
    
    QProcess *sender = dynamic_cast<QProcess*>(QObject::sender());
    if (sender) {
        ProcessHandle_t handle = (ProcessHandle_t)sender->property("ProcessHandle").toUInt();
        
        if (m_processMap.contains(handle)) {
            // Emit event BEFORE removing descriptor so slots can access it
            emit ProcessEvent(handle, PROCESS_STOPPED);
            
            m_processMap[handle]->deleteLater();
            m_processMap.remove(handle);
            m_processDescriptors.remove(handle);
            
            qDebug() << "QProcessManager: Process stopped. Handle:" << handle 
                     << "ExitCode:" << exitCode 
                     << "ExitStatus:" << exitStatus;
        } else {
            qWarning() << "QProcessManager: Can't find process with handle:" << handle;
        }
    }
    
    blockSignals(false);
    
    // Check if all processes finished
    if (m_processMap.isEmpty()) {
        qDebug() << "QProcessManager: All processes finished";
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
