#ifndef QPROCESSMANAGER_H
#define QPROCESSMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QProcess>

namespace Daqster {

/**
 * @brief Generic process manager for launching and managing child processes
 * 
 * Provides core functionality for process lifecycle management including:
 * - Starting/stopping processes
 * - Process tracking with handle-based identification
 * - Graceful termination with fallback to force kill
 * - Process descriptor storage
 * - Event notifications for process state changes
 * 
 * This is a base class intended to be extended with application-specific
 * environment setup and process management logic.
 */
class QProcessManager : public QObject
{
    Q_OBJECT

public:
    typedef enum {
        PROCESS_NA,
        PROCESS_STARTED,
        PROCESS_STOPPED
    } ProcessEvent_t;

    typedef struct {
        QString            Name;
        QStringList        Arguments;
        QProcess::OpenMode Mode;
    } ProcessDescriptor_t;

    typedef uint32_t ProcessHandle_t;

    explicit QProcessManager(QObject *parent = nullptr);
    virtual ~QProcessManager();

    /**
     * @brief Get process descriptor by handle
     * @param handle Process handle
     * @param desc Reference to store descriptor
     * @return true if descriptor found, false otherwise
     */
    bool GetProcessDescriptor(const ProcessHandle_t& handle, ProcessDescriptor_t& desc) const;

public slots:
    /**
     * @brief Start a new process
     * @param name Process executable path or name
     * @param arguments Command line arguments
     * @param mode Process open mode (default: ReadWrite)
     * 
     * Subclasses should override setupProcessEnvironment() to customize
     * the process environment before starting.
     */
    virtual void StartProcess(const QString& name, 
                             const QStringList& arguments, 
                             QProcess::OpenMode mode = QProcess::ReadWrite);

    /**
     * @brief Terminate all managed processes
     * 
     * Attempts graceful termination first (10 second timeout),
     * then forces kill if process doesn't respond.
     */
    void KillAll();

signals:
    /**
     * @brief Emitted when a process changes state
     * @param handle Process handle
     * @param event Event type (started/stopped)
     */
    void ProcessEvent(const QProcessManager::ProcessHandle_t& handle, 
                     const QProcessManager::ProcessEvent_t& event);

protected slots:
    /**
     * @brief Handle process termination
     * @param exitCode Process exit code
     * @param exitStatus Process exit status
     * 
     * Called automatically when a managed process finishes.
     * Subclasses can override to add custom cleanup logic.
     */
    virtual void OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

protected:
    /**
     * @brief Setup process environment before starting
     * @param process Process to configure
     * @param name Process name/path
     * @param arguments Process arguments
     * 
     * Override this in subclasses to customize environment variables,
     * working directory, etc. Default implementation does nothing.
     */
    virtual void setupProcessEnvironment(QProcess* process, 
                                        const QString& name,
                                        const QStringList& arguments);

    /**
     * @brief Called when all processes have finished
     * 
     * Override in subclasses for custom behavior (e.g., quit application).
     * Default implementation does nothing.
     */
    virtual void onAllProcessesFinished();

    ProcessHandle_t m_nextHandle;
    QMap<ProcessHandle_t, QProcess*> m_processMap;
    QMap<ProcessHandle_t, ProcessDescriptor_t> m_processDescriptors;
};

} // namespace Daqster

#endif // QPROCESSMANAGER_H
