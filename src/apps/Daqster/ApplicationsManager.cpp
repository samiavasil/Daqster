#include "ApplicationsManager.h"
#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QApplication>

ApplicationsManager *ApplicationsManager::m_Manager;

ApplicationsManager::ApplicationsManager() : QObject(nullptr), nextHndl(0) {}

ApplicationsManager::~ApplicationsManager() { KillAll(); }

ApplicationsManager &ApplicationsManager::Instance() {
  if (nullptr == m_Manager) {
    m_Manager = new ApplicationsManager();
  }
  return *m_Manager;
}
#include<ostream>
void ApplicationsManager::KillAll() {
  blockSignals(true);
  
  auto iter = m_ProcessMap.begin();
  while (iter != m_ProcessMap.end()) {
    if (nullptr != iter.value()) {
      // Send quit signal to the app
      QProcess* proccess = iter.value();
      QString data = QString::fromStdString(std::string("quit\r\n"));
      qDebug() << "Write data: " << proccess->write(data.toLocal8Bit()) << " bytes";
      if (iter.value()->waitForFinished(10000)) {
        qDebug() << "Process stoped";
      } else {
        qDebug() << "Can't stop Process Try to kill: " << iter.value()->program();
        iter.value()->kill();
      }

    }
    iter = m_ProcessMap.erase(iter);
  }
  m_ProcessDescs.clear();
  blockSignals(false);
}

void ApplicationsManager::StartApplication(const QString &Name,
                                           const QStringList &Arguments,
                                           QProcess::OpenMode Mode) {
  QProcess *newProc = new QProcess(this);
  if (newProc) {
    AppDescriptor_t Desc = {Name, Arguments, Mode};
    qDebug() << "App: " << Name << "Args: " << Arguments;
    newProc->setInputChannelMode(QProcess::ManagedInputChannel);
    
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
    
    newProc->setProcessEnvironment(env);
    
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
    
    newProc->start(Name, Arguments, Mode);
    m_ProcessMap[nextHndl] = newProc;
    m_ProcessDescs[nextHndl] = Desc;
    newProc->setProperty("AppHndl", nextHndl);
    connect(newProc, SIGNAL(finished(int, QProcess::ExitStatus)), this,
            SLOT(AppFinished(int, QProcess::ExitStatus)));
    qDebug() << "Process '" << Name << "' Start: Hndl " << nextHndl;
    newProc->waitForStarted();

    emit ApplicationEvent(nextHndl, APP_STARTED);
    nextHndl++;
  } else {
    qDebug() << "Can't start process '" << Name << "' Start";
  }
}

void ApplicationsManager::AppFinished(int exitCode,
                                      QProcess::ExitStatus exitStatus) {

  blockSignals(true);
  QProcess *sender = dynamic_cast<QProcess *>(QObject::sender());
  if (sender) {
    AppHndl_t apHndl = (AppHndl_t)sender->property("AppHndl").toUInt();
    if (m_ProcessMap.contains(apHndl)) {
      m_ProcessMap[apHndl]->deleteLater();
      m_ProcessMap[apHndl] = nullptr;
      qDebug() << "Process Stop: Hndl" << apHndl << " With exitCode "
               << exitCode << ", exitStatus " << exitStatus;
    } else {
      qDebug() << "Can't find process with Hndl " << apHndl;
    }
  }
  blockSignals(false);
}

bool ApplicationsManager::GetAppDescryptor(const AppHndl_t &Hndl,
                                           AppDescriptor_t &Desc) {
  bool ret = false;
  if (m_ProcessDescs.contains(nextHndl)) {
    Desc = m_ProcessDescs[Hndl];
    ret = true;
  }
  return ret;
}
