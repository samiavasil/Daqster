#include "ApplicationsManager.h"
#include <QProcessManager.h>
#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QApplication>

ApplicationsManager *ApplicationsManager::m_Manager = nullptr;

ApplicationsManager::ApplicationsManager() 
    : Daqster::QProcessManager(nullptr),
      m_headlessMode(false)
{
    // Forward ProcessEvent signal to ApplicationEvent signal with compatible types
    connect(this, &QProcessManager::ProcessEvent, 
            this, [this](ProcessHandle_t handle, ProcessEvent_t event) {
                AppEvent_t appEvent = static_cast<AppEvent_t>(event);
                emit ApplicationEvent(handle, appEvent);
            });
}

ApplicationsManager::~ApplicationsManager() 
{
    KillAll();
}

ApplicationsManager &ApplicationsManager::Instance() {
    if (nullptr == m_Manager) {
        m_Manager = new ApplicationsManager();
    }
    return *m_Manager;
}

void ApplicationsManager::SetHeadlessMode(bool enabled) {
    m_headlessMode = enabled;
}

bool ApplicationsManager::GetAppDescryptor(const AppHndl_t& Hndl, 
                                          AppDescriptor_t& Desc) const
{
    return GetProcessDescriptor(Hndl, Desc);
}

void ApplicationsManager::StartApplication(const QString& Name, 
                                          const QStringList& Arguments, 
                                          QProcess::OpenMode Mode)
{
    // Delegate to base class StartProcess
    StartProcess(Name, Arguments, Mode);
}
void ApplicationsManager::setupProcessEnvironment(QProcess* process, 
                                                 const QString& name,
                                                 const QStringList& arguments)
{
    Q_UNUSED(name);
    Q_UNUSED(arguments);
    
    // Set environment variables for child process
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    // Check if we're in Daqster AppImage environment
    QString appImageEnv = env.value("APPIMAGE");
    QString basePath;
    
    qDebug() << "APPIMAGE env var:" << appImageEnv;
    qDebug() << "Current working directory:" << QDir::currentPath();
    qDebug() << "Application directory:" << qApp->applicationDirPath();
    
    // Check if we're in Daqster AppImage (not just any AppImage like Cursor IDE)
    bool isDaqsterAppImage = !appImageEnv.isEmpty() && appImageEnv.contains("Daqster");
    
    if (isDaqsterAppImage) {
      // We're in Daqster AppImage - use AppImage internal paths
      // In AppImage, applicationDirPath() should be the mounted AppImage path
      basePath = qApp->applicationDirPath();
      qDebug() << "Daqster AppImage environment detected, base path:" << basePath;
    } else {
      // We're in regular build - use application directory
      basePath = qApp->applicationDirPath();
      qDebug() << "Regular build environment, base path:" << basePath;
    }
    
    // Set library paths
    if (isDaqsterAppImage) {
      // AppImage structure
      env.insert("LD_LIBRARY_PATH", basePath + "/usr/lib:" + env.value("LD_LIBRARY_PATH"));
      env.insert("QML2_IMPORT_PATH", basePath + "/usr/lib/qml:" + env.value("QML2_IMPORT_PATH"));
      env.insert("QT_PLUGIN_PATH", basePath + "/usr/lib/plugins:" + env.value("QT_PLUGIN_PATH"));
      env.insert("QT_QPA_PLATFORM_PLUGIN_PATH", basePath + "/usr/lib/plugins/platforms");
      env.insert("DAQSTER_PLUGIN_DIR", basePath + "/usr/lib/daqster/plugins");
      env.insert("DAQSTER_PLUGIN_PATH", basePath + "/usr/lib/daqster/plugins:" + QDir::homePath() + "/.local/share/daqster/plugins");
    } else {
      // Regular build structure
      env.insert("LD_LIBRARY_PATH", basePath + "/../lib:" + env.value("LD_LIBRARY_PATH"));
      env.insert("QML2_IMPORT_PATH", basePath + "/../lib/qml:" + env.value("QML2_IMPORT_PATH"));
      env.insert("QT_PLUGIN_PATH", basePath + "/../lib/plugins:" + env.value("QT_PLUGIN_PATH"));
      env.insert("QT_QPA_PLATFORM_PLUGIN_PATH", basePath + "/../lib/plugins/platforms");
      env.insert("DAQSTER_PLUGIN_DIR", basePath + "/plugins");
      env.insert("DAQSTER_PLUGIN_PATH", basePath + "/plugins:" + QDir::homePath() + "/.local/share/daqster/plugins");
    }
    
    // Set writable directories
    env.insert("XDG_CONFIG_HOME", QDir::homePath() + "/.config/daqster");
    env.insert("XDG_DATA_HOME", QDir::homePath() + "/.local/share/daqster");
    env.insert("XDG_CACHE_HOME", QDir::homePath() + "/.cache/daqster");
    
    // Create directories
    QDir().mkpath(env.value("XDG_CONFIG_HOME"));
    QDir().mkpath(env.value("XDG_DATA_HOME"));
    QDir().mkpath(env.value("XDG_CACHE_HOME"));
    
    process->setProcessEnvironment(env);
    
    // Debug: Print environment variables
    qDebug() << "=== Environment Variables for Child Process ===";
    qDebug() << "LD_LIBRARY_PATH:" << env.value("LD_LIBRARY_PATH");
    qDebug() << "QML2_IMPORT_PATH:" << env.value("QML2_IMPORT_PATH");
    qDebug() << "QT_PLUGIN_PATH:" << env.value("QT_PLUGIN_PATH");
    qDebug() << "QT_QPA_PLATFORM_PLUGIN_PATH:" << env.value("QT_QPA_PLATFORM_PLUGIN_PATH");
    qDebug() << "DAQSTER_PLUGIN_DIR:" << env.value("DAQSTER_PLUGIN_DIR");
    qDebug() << "DAQSTER_PLUGIN_PATH:" << env.value("DAQSTER_PLUGIN_PATH");
    qDebug() << "XDG_CONFIG_HOME:" << env.value("XDG_CONFIG_HOME");
    qDebug() << "XDG_DATA_HOME:" << env.value("XDG_DATA_HOME");
    qDebug() << "XDG_CACHE_HOME:" << env.value("XDG_CACHE_HOME");
    qDebug() << "=== End Environment Variables ===";
}

// Override base class method to forward events with ApplicationsManager signature
void ApplicationsManager::OnProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    // First call base class to handle cleanup and emit ProcessEvent
    QProcessManager::OnProcessFinished(exitCode, exitStatus);
    
    // Base class already emits ProcessEvent, which is what ApplicationEvent connects to
    // No need to emit again - the signal connection will handle forwarding
}

// Override to implement headless mode quit logic
void ApplicationsManager::onAllProcessesFinished() {
  if (m_headlessMode) {
    qDebug() << "All child processes finished in headless mode. Exiting...";
    qApp->quit();
  }
}
