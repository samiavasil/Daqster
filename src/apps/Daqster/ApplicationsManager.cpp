#include "ApplicationsManager.h"
#include "LogCategories.h"
#include <QProcessManager.h>
#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QApplication>


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
  qCDebug(lcApp) << "ApplicationsManager destructor: invoking KillAll()";
  KillAll();
}

ApplicationsManager &ApplicationsManager::Instance() {
    static ApplicationsManager s_manager;
    return s_manager;
}

void ApplicationsManager::SetHeadlessMode(bool enabled) {
    m_headlessMode = enabled;
}

bool ApplicationsManager::GetAppDescryptor(const AppHndl_t& Hndl, 
                                          AppDescriptor_t& Desc) const
{
    return GetProcessDescriptor(Hndl, Desc);
}

void ApplicationsManager::KillApp(const AppHndl_t& handle) {
    Kill(handle);
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
    
    // Start from system environment but preserve Qt-related variables from the
    // current process (which may have been modified via qputenv for theming, etc.)
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    // Preserve Qt environment variables from current process that affect appearance/behavior
    static const char* const qtEnvVars[] = {
        "QT_QPA_PLATFORMTHEME", "QT_STYLE_OVERRIDE", "QT_QPA_PLATFORM",
        "QT_AUTO_SCREEN_SCALE_FACTOR", "QT_SCALE_FACTOR", "QT_FONT_DPI",
        "QT_SCREEN_SCALE_FACTORS", "QT_QPA_PLATFORMTHEME", "QT_WAYLAND_DISABLE_WINDOWDECORATION",
        "QT_LOGGING_RULES", "QT_MESSAGE_PATTERN",
        nullptr
    };
    for (int i = 0; qtEnvVars[i]; ++i) {
        QByteArray val = qgetenv(qtEnvVars[i]);
        if (!val.isEmpty()) {
            env.insert(qtEnvVars[i], QString::fromUtf8(val));
        }
    }
    
    QString basePath = qApp->applicationDirPath();
    
    qCDebug(lcApp) << "Current working directory:" << QDir::currentPath();
    qCDebug(lcApp) << "Application directory:" << basePath;
    
#ifdef Q_OS_WIN
    // Windows: add plugin/lib dirs to PATH for DLL search
    QString pluginDir = basePath + "/plugins";
    QString libDir = basePath + "/../lib";
    env.insert("PATH", pluginDir + ";" + libDir + ";" + env.value("PATH"));
    
    // User data dirs via %APPDATA%
    QString appData = QDir::homePath() + "/AppData/Roaming/Daqster";
    env.insert("DAQSTER_PLUGIN_DIR", pluginDir);
    env.insert("DAQSTER_PLUGIN_PATH", pluginDir + ";" + libDir);
    
    QDir().mkpath(appData + "/config");
    QDir().mkpath(appData + "/data");
    QDir().mkpath(appData + "/cache");
#else
    // Linux: AppImage detection
    QString appImageEnv = env.value("APPIMAGE");
    bool isDaqsterAppImage = !appImageEnv.isEmpty() && appImageEnv.contains("Daqster");
    qCDebug(lcApp) << "APPIMAGE env var:" << appImageEnv;
    
    // The Daqster framework libraries and plugins are in the application directory (bin/),
    // not in lib/. The RUNPATH of the executable includes bin/, but when we override
    // LD_LIBRARY_PATH we must include bin/ first, otherwise plugin loading fails
    // because dependencies (libFrameworkGui, libNodeEditorLibrary, etc.) are in bin/.
    const QString binPath = QDir(basePath).absolutePath();
    const QString libPath = QDir(basePath + "/../lib").absolutePath();
    const QString ldLibraryPath = binPath + ":" + libPath + ":" + env.value("LD_LIBRARY_PATH");
    
    if (isDaqsterAppImage) {
      // AppImage structure
      env.insert("LD_LIBRARY_PATH", ldLibraryPath);
      env.insert("QML2_IMPORT_PATH", QDir(basePath + "/../lib/qml").absolutePath() + ":" + env.value("QML2_IMPORT_PATH"));
      env.insert("QT_PLUGIN_PATH", QDir(basePath + "/../lib/plugins").absolutePath() + ":" + env.value("QT_PLUGIN_PATH"));
      env.insert("QT_QPA_PLATFORM_PLUGIN_PATH", QDir(basePath + "/../lib/plugins/platforms").absolutePath());
      env.insert("DAQSTER_PLUGIN_DIR", QDir(basePath + "/../lib/daqster/plugins").absolutePath());
      env.insert("DAQSTER_PLUGIN_PATH", QDir(basePath + "/../lib/daqster/plugins").absolutePath() + ":" + QDir::homePath() + "/.local/share/daqster/plugins");
    } else {
      // Regular build structure
      env.insert("LD_LIBRARY_PATH", ldLibraryPath);
      env.insert("QML2_IMPORT_PATH", basePath + "/../lib/qml:" + env.value("QML2_IMPORT_PATH"));
      env.insert("QT_PLUGIN_PATH", basePath + "/../lib/plugins:" + env.value("QT_PLUGIN_PATH"));
      env.insert("QT_QPA_PLATFORM_PLUGIN_PATH", basePath + "/../lib/plugins/platforms");
      env.insert("DAQSTER_PLUGIN_DIR", basePath + "/plugins");
      env.insert("DAQSTER_PLUGIN_PATH", basePath + "/plugins:" + QDir::homePath() + "/.local/share/daqster/plugins");
    }
    
    // XDG dirs (Linux only)
    env.insert("XDG_CONFIG_HOME", QDir::homePath() + "/.config/daqster");
    env.insert("XDG_DATA_HOME", QDir::homePath() + "/.local/share/daqster");
    env.insert("XDG_CACHE_HOME", QDir::homePath() + "/.cache/daqster");
    
    QDir().mkpath(env.value("XDG_CONFIG_HOME"));
    QDir().mkpath(env.value("XDG_DATA_HOME"));
    QDir().mkpath(env.value("XDG_CACHE_HOME"));
#endif
    
    process->setProcessEnvironment(env);
    
    // Debug output
    qCDebug(lcApp) << "=== Environment Variables for Child Process ===";
    qCDebug(lcApp) << "DAQSTER_PLUGIN_DIR:" << env.value("DAQSTER_PLUGIN_DIR");
#ifdef Q_OS_WIN
    qCDebug(lcApp) << "PATH (first 200 chars):" << env.value("PATH").left(200);
#else
    qCDebug(lcApp) << "LD_LIBRARY_PATH:" << env.value("LD_LIBRARY_PATH");
    qCDebug(lcApp) << "XDG_CONFIG_HOME:" << env.value("XDG_CONFIG_HOME");
#endif
    qCDebug(lcApp) << "=== End Environment Variables ===";
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
    qCDebug(lcApp) << "ApplicationsManager::onAllProcessesFinished(): all child processes finished in headless mode, calling qApp->quit()";
    qApp->quit();
  }
}
