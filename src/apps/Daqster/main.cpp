#include "ApplicationsManager.h"
#include "QPluginManager.h"

#include <AppToolbar.h>
#include <QApplication>
#include <QBasePluginObject.h>
#include <QCommandLineParser>
#include <QFile>
#include <QDir>
#include "debug.h"
#include "main.h"

#ifdef Q_OS_WIN
#include "WindowsShutdownHandler.h"
#else
#include "UnixShutdownHandler.h"
#endif

class msg {
public:
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
  static void myMessageOutput(QtMsgType type, const char *b) {
    QString msg;
    msg.sprintf("%s", b);
#else
  static void myMessageOutput(QtMsgType type, const QMessageLogContext &,
                              const QString &msg) {
#endif

    switch (type) {
    case QtDebugMsg:
      fprintf(stderr, "%s\n", msg.toStdString().c_str());
      break;
    case QtWarningMsg:
      // TODO:   fprintf(stderr, "%s\n", msg);
      break;
    case QtCriticalMsg:
      fprintf(stderr, "%s\n", msg.toStdString().c_str());
      break;
    case QtFatalMsg:
      fprintf(stderr, "%s\n", msg.toStdString().c_str());
      abort();
    }
  }
};

void PluginsInit() {
  /*TODO:  Move this on some initialization routine*/
  Daqster::QPluginManager *PluginManager = Daqster::QPluginManager::instance();

  if (nullptr != PluginManager) {
    PluginManager->SearchForPlugins();
    qDebug() << "Plugin Manager: " << PluginManager;
    QList<Daqster::PluginDescription> PluginsList =
        PluginManager->GetPluginList();
    /*Just try to load/unload all plugins in initialization phase*/
    foreach (const Daqster::PluginDescription &Desc, PluginsList) {
      Daqster::QBasePluginObject* obj = PluginManager->CreatePluginObject(
          Desc.GetProperty(PLUGIN_HASH).toString(), nullptr);
      if (obj != nullptr)
        obj->deleteLater();
    }
  }
}

int main(int argc, char *argv[]) {

  int res = 0;
  //    msg m;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
  qInstallMsgHandler(m.myMessageOutput);
#else
  //   qInstallMessageHandler(m.myMessageOutput);
#endif
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  //  // detach from the current console window
  //   // if launched from a console window, that will still run waiting for the
  //   new console (below) to close
  //   // it is useful to detach from Qt Creator's <Application output> panel
  //   FreeConsole();

  //   // create a separate new console window
  //   AllocConsole();

  //   // attach the new console to this application's process
  //   AttachConsole(GetCurrentProcessId());

  // TODO: Check argument parser: http://doc.qt.io/qt-5/qcommandlineparser.html
  QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
  QApplication a(argc, argv);
  QApplication::setApplicationName("Daqster");
  QApplication::setApplicationVersion("0.1");

  // Setup platform-specific shutdown handler
#ifdef Q_OS_WIN
  WindowsShutdownHandler *shutdownHandler = new WindowsShutdownHandler(&a);
#else
  UnixShutdownHandler *shutdownHandler = new UnixShutdownHandler(&a);
#endif
  
  if (!shutdownHandler->initialize()) {
    qWarning() << "Failed to initialize shutdown handler";
  }
  
  QObject::connect(shutdownHandler, &ShutdownHandler::shutdownRequested, [&a]() {
    // Stop all child processes gracefully
    ApplicationsManager::Instance().KillAll();
    // Stop the application
    a.quit();
  });

  QCommandLineParser parser;
  parser.setApplicationDescription(
      "This program is used to run Daqster Application plugins");
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addPositionalArgument(
      "apps ...", QCoreApplication::translate(
                      "main", "One ore more Application plugin names which "
                              "will be automaticaly started"));
  // An option with a value
  QCommandLineOption targetDirectoryOption(
      QStringList() << "t"
                    << "target-directory",
      QCoreApplication::translate("main",
                                  "Copy all source files into <directory>."),
      QCoreApplication::translate("main", "directory"));
  parser.addOption(targetDirectoryOption);

  // Process the actual command line arguments given by the user
  parser.process(a);

  const QStringList args = parser.positionalArguments();

  qDebug() << "Positional Argumments: " << args;

  Daqster::QPluginManager *PluginManager = Daqster::QPluginManager::instance();
  // For correct plugoins shutdown behaviour QPluginManager initialization
  // should be called.
  if (!PluginManager->Initialize()) {
    DEBUG << "QPluginManager Initialization Error";
    return 1;
  }

  DEBUG << "Show window";
  PluginsInit();
  qDebug() << "ARGS: " << args;
  
  // Debug: Print current environment and paths
  qDebug() << "=== Application Environment Debug ===";
  qDebug() << "APPIMAGE env var:" << qgetenv("APPIMAGE");
  qDebug() << "Current working directory:" << QDir::currentPath();
  qDebug() << "Application directory:" << qApp->applicationDirPath();
  qDebug() << "=== End Application Environment Debug ===";
  
  // Define Filter outside if/else scope so it can be used in both sections
  Daqster::PluginFilter Filter;
  Filter.AddFilter(
      PLUGIN_TYPE,
      QString("%1").arg(Daqster::PluginDescription::APPLICATION_PLUGIN));

  QList<Daqster::PluginDescription> PluginsList = PluginManager->GetPluginList(Filter);
  qDebug() << "PluginsList count: " << PluginsList.count();
  int ctr = 0;
  foreach (const Daqster::PluginDescription &Desc, PluginsList) {
    ctr++;
    qDebug() << "  Plugin" << ctr << ": " << Desc.GetProperty(PLUGIN_NAME).toString();
    qDebug() << "  Location" << ctr << ": " << Desc.GetProperty(PLUGIN_LOCATION).toString();

  }

  if (args.count() > 0) {
    if (args.count() > 1) {
      // Multi-plugin mode: start multiple child processes
      ApplicationsManager::Instance().SetHeadlessMode(true);
      foreach (auto Name, args) {
        // Try multiple approaches for starting the application
        QString executablePath;
        
        // 1. Check if we're in AppImage and AppRun exists
        QString appImageEnv = qgetenv("APPIMAGE");
        if (!appImageEnv.isEmpty()) {
          QString appImagePath = qApp->applicationDirPath() + "/../AppRun";
          if (QFile::exists(appImagePath)) {
            executablePath = appImagePath;
            qDebug() << "Using AppRun script for AppImage environment";
          }
        }
        
        // 2. If no AppRun found, try direct executable
        if (executablePath.isEmpty()) {
          executablePath = "./Daqster";
          qDebug() << "Using direct executable";
        }
        
        ApplicationsManager::Instance().StartApplication(executablePath, QStringList(Name));
        qDebug() << "Start Application: " << Name << " via " << executablePath;
      }
    } else {
      // Single plugin mode: run plugin directly in this process
      QString Name = args[0];
      Daqster::QBasePluginObject *obj = nullptr;
      qDebug() << "\nSearch for plugin: " << Name;
      int ctr = 0;
      foreach (const Daqster::PluginDescription &Desc, PluginsList) {
        ctr++;
        qDebug() << "  Plug" << ctr << ": "
                 << Desc.GetProperty(PLUGIN_NAME).toString();
        if (0 == Desc.GetProperty(PLUGIN_NAME).toString().compare(Name, Qt::CaseInsensitive)) {
          obj = PluginManager->CreatePluginObject(
              Desc.GetProperty(PLUGIN_HASH).toString(), nullptr);
          if (nullptr != obj) {
            qDebug() << "Plugin " << Name << " founded! Run it.";
            obj->Initialize();
            QApplication::setApplicationName(
                Desc.GetProperty(PLUGIN_HASH).toString());
            break;
          }
        }
      }
      
      // Check if plugin was found
      if (nullptr == obj) {
        qCritical() << "Error: Plugin '" << Name << "' not found!";
        return 1;
      }
    }
    res = a.exec();
  } else {
    // GUI mode: show AppToolbar
    qDebug() << __BASE_FILE__ << __FILE__;
    AppToolbar AppBar;
    AppBar.show();
    res = a.exec();
  }

  // Daqster::QPluginManager::instance()->ShutdownPluginManager();
  return res;
}
