#ifndef APPLICATIONSMANAGER_H
#define APPLICATIONSMANAGER_H

#include "QProcessManager.h"
#include "main.h"

/**
 * @brief Daqster-specific process manager with plugin environment setup
 * 
 * Extends QProcessManager with Daqster-specific functionality:
 * - AppImage environment detection
 * - Daqster plugin paths configuration
 * - XDG directory setup for Daqster
 * - Headless mode for multi-plugin launches
 * - Singleton pattern
 */
class ApplicationsManager : public Daqster::QProcessManager
{
    Q_OBJECT

public:
    // Type aliases for backward compatibility
    typedef ProcessEvent_t AppEvent_t;
    typedef ProcessDescriptor_t AppDescriptor_t;
    typedef ProcessHandle_t AppHndl_t;
    
    // Keep original enum names for compatibility
    static constexpr ProcessEvent_t APP_NA = PROCESS_NA;
    static constexpr ProcessEvent_t APP_STARTED = PROCESS_STARTED;
    static constexpr ProcessEvent_t APP_STOPED = PROCESS_STOPPED;

    static ApplicationsManager& Instance();
    ~ApplicationsManager() override;

    bool GetAppDescryptor(const AppHndl_t& Hndl, AppDescriptor_t& Desc) const;
    void SetHeadlessMode(bool enabled);

public slots:
    void StartApplication(const QString& Name, 
                         const QStringList& Arguments, 
                         QProcess::OpenMode Mode = QProcess::ReadWrite);

signals:
    void ApplicationEvent(const ApplicationsManager::AppHndl_t& AppHnd, 
                         const ApplicationsManager::AppEvent_t& ev);

protected:
    void setupProcessEnvironment(QProcess* process, 
                                const QString& name,
                                const QStringList& arguments) override;
    
    void onAllProcessesFinished() override;
    
    void OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) override;

private:
    ApplicationsManager();
    
    static ApplicationsManager* m_Manager;
    bool m_headlessMode;
};

#endif // APPLICATIONSMANAGER_H
