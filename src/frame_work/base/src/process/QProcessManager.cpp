#include "QProcessManager.h"
#include "LogManager.h"
#include "LogCategories.h"
#include <QDebug>
#include <QVariant>
#include <QUuid>
#include <cstdio>

namespace Daqster {

QProcessManager::QProcessManager(QObject *parent)
    : QObject(parent)
    , m_nextHandle(0)
{
}

QProcessManager::~QProcessManager()
{
    qCDebug(lcProcess) << "QProcessManager destructor: invoking KillAll()";
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
        // Generate instance ID for this child process
        QString instanceId = QUuid::createUuid().toString(QUuid::Id128).left(8);

        QStringList args = arguments;
        args << "--instance-id" << instanceId;

        // Forward parent's log settings so child inherits them
        LogManager *parentLog = LogManager::instance();
        if (parentLog) {
            args << "--log-console-enabled" << (parentLog->isConsoleEnabled() ? "1" : "0");
            args << "--log-level" << parentLog->consoleLogLevelName();
            // Forward current filter rules so child categories match parent's settings
            QString rules = parentLog->filterRules();
            if (!rules.isEmpty()) {
                args << "--log-rules" << rules;
            }
        }

        ProcessDescriptor_t desc = {name, args, mode};
        qCDebug(lcProcess) << "QProcessManager: Starting process:" << name << "Args:" << args;
        
        newProc->setInputChannelMode(QProcess::ManagedInputChannel);
        
        // Allow subclass to customize environment
        setupProcessEnvironment(newProc, name, args);
        
        // Start the process
        newProc->start(name, args, mode);
        
        // Store process info
        m_processMap[m_nextHandle] = newProc;
        m_processDescriptors[m_nextHandle] = desc;
        newProc->setProperty("ProcessHandle", QVariant::fromValue(static_cast<unsigned int>(m_nextHandle)));
        
        // Connect finished signal
        connect(newProc, SIGNAL(finished(int, QProcess::ExitStatus)), 
                this, SLOT(OnProcessFinished(int, QProcess::ExitStatus)));

        // Capture child process stderr and forward to LogManager
        // If child output is already formatted (starts with '['), pass through directly
        // to avoid double-formatting. Otherwise, format as raw child output.
        connect(newProc, &QProcess::readyReadStandardError, this, [newProc, instanceId]() {
            QByteArray output = newProc->readAllStandardError();
            if (!output.isEmpty()) {
                QString text = QString::fromLocal8Bit(output).trimmed();
                if (text.startsWith('[')) {
                    // Already formatted by child's LogManager — write directly to stderr
                    fprintf(stderr, "%s\n", text.toUtf8().constData());
                    fflush(stderr);
                } else {
                    // Raw output — format with our handler, include instance ID
                    qCInfo(lcProcess) << "[child:" << instanceId << "]" << text;
                }
            }
        });

        // Also capture stdout
        connect(newProc, &QProcess::readyReadStandardOutput, this, [newProc, instanceId]() {
            QByteArray output = newProc->readAllStandardOutput();
            if (!output.isEmpty()) {
                QString text = QString::fromLocal8Bit(output).trimmed();
                if (text.startsWith('[')) {
                    // Already formatted — write directly to stdout
                    fprintf(stdout, "%s\n", text.toUtf8().constData());
                    fflush(stdout);
                } else {
                    // Raw output — format with our handler, include instance ID
                    qCInfo(lcProcess) << "[child:" << instanceId << ":out]" << text;
                }
            }
        });
        
        qCDebug(lcProcess) << "QProcessManager: Process started with handle:" << m_nextHandle;
        if (!newProc->waitForStarted(5000)) {
            qCWarning(lcProcess) << "QProcessManager: Process failed to start within 5s:" << name;
        }

        emit ProcessEvent(m_nextHandle, PROCESS_STARTED);
        m_nextHandle++;
    } else {
        qCWarning(lcProcess) << "QProcessManager: Failed to create QProcess for:" << name;
    }
}

void QProcessManager::KillAll()
{
    qCDebug(lcProcess) << "QProcessManager::KillAll() called";
    
    // Snapshot the keys so we can iterate safely while OnProcessFinished modifies the map
    QList<ProcessHandle_t> handles = m_processMap.keys();
    
    for (const ProcessHandle_t& handle : handles) {
        if (m_processMap.contains(handle)) {
            QProcess* process = m_processMap[handle];
            if (nullptr != process) {
                qCDebug(lcProcess) << "QProcessManager: Requesting graceful shutdown for:"
                         << process->program();
                process->terminate();
                
                if (process->waitForFinished(10000)) {
                    qCDebug(lcProcess) << "QProcessManager: Process stopped gracefully";
                } else {
                    qCWarning(lcProcess) << "QProcessManager: Process did not respond, forcing kill:"
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
        qCWarning(lcProcess) << "QProcessManager::Kill(): unknown handle:" << handle;
        return;
    }

    QProcess* process = m_processMap[handle];
    qCDebug(lcProcess) << "QProcessManager::Kill() handle:" << handle << "process:" << process->program();

    process->terminate();
    if (process->waitForFinished(10000)) {
        qCDebug(lcProcess) << "QProcessManager: Process" << handle << "stopped gracefully";
    } else {
        qCWarning(lcProcess) << "QProcessManager: Process" << handle << "did not respond, forcing kill";
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
            
            qCDebug(lcProcess) << "QProcessManager: Process stopped. Handle:" << handle 
                     << "ExitCode:" << exitCode 
                     << "ExitStatus:" << exitStatus;

            // Emit AFTER cleanup so slots don't see stale data
            emit ProcessEvent(handle, PROCESS_STOPPED);
        } else {
            qCWarning(lcProcess) << "QProcessManager: Can't find process with handle:" << handle;
        }
    }
    
    // Check if all processes finished
    if (m_processMap.isEmpty()) {
        qCDebug(lcProcess) << "QProcessManager: All processes finished";
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
