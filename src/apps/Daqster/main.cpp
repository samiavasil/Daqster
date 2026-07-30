#include "ApplicationsManager.h"
#include "QPluginManager.h"
#include "ShutdownHandler.h"
#include "LogManager.h"

#include <AppToolbar.h>
#include <QApplication>
#include <QBasePluginObject.h>
#include <QCommandLineParser>
#include <QFile>
#include <QMainWindow>
#include <QDir>
#include "LogCategories.h"
#include <QSettings>
#include <QLoggingCategory>
#include "QConsoleListener.h"
#include "main.h"

void PluginsInit() {
  /*TODO:  Move this on some initialization routine*/
  Daqster::QPluginManager *PluginManager = Daqster::QPluginManager::instance();

  if (nullptr != PluginManager) {
    PluginManager->SearchForPlugins();
    qCDebug(lcApp) << "Plugin Manager: " << PluginManager;
    //  PluginManager->SearchForPlugins();
    // PluginManager->ShowPluginManagerGui();
    QList<Daqster::PluginDescription> PluginsList =
        PluginManager->GetPluginList();
    /*Just try to load/unload all plugins in initialization phase*/
    foreach (const Daqster::PluginDescription &Desc, PluginsList) {
      if (!Desc.IsEnabled()) continue;  // Skip disabled plugins
      for (int i = 0; i < 1; i++) {
        Daqster::QBasePluginObject* obj = PluginManager->CreatePluginObject(Desc.GetProperty(PLUGIN_HASH).toString(),nullptr);
        if(obj != NULL)
          obj->deleteLater();
      }
    }
  }
}

int main(int argc, char *argv[]) {

  int res = 0;

  Daqster::LogManager::instance()->initialize();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
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

  auto *shutdownHandler = ShutdownHandler::create(&a);
  shutdownHandler->initialize();
  QObject::connect(shutdownHandler, &ShutdownHandler::shutdownRequested, &a, &QCoreApplication::quit);

  // Load theme if configured (default: system/light)
  QSettings appSettings("Daqster", "Daqster");
  QString theme = appSettings.value("Theme/Style", "default").toString();
  if (theme == "dark") {
      QFile styleFile(":/toolbar/icons/StyleFile");
      if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
          a.setStyleSheet(styleFile.readAll());
          styleFile.close();
      }
  }

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

  QCommandLineOption instanceIdOption(
      QStringList() << "instance-id",
      "Child process instance identifier (internal)",
      "id");
  parser.addOption(instanceIdOption);

  QCommandLineOption logConsoleOption(
      QStringList() << "log-console-enabled",
      "Enable console logging in child process (internal)",
      "0|1");
  parser.addOption(logConsoleOption);

  QCommandLineOption logLevelOption(
      QStringList() << "log-level",
      "Minimum console log level (internal)",
      "level");
  parser.addOption(logLevelOption);

  QCommandLineOption logRulesOption(
      QStringList() << "log-rules",
      "Qt logging category rules (internal)",
      "rules");
  parser.addOption(logRulesOption);

  // Process the actual command line arguments given by the user
  parser.process(a);

  if (parser.isSet(instanceIdOption)) {
      Daqster::LogManager::instance()->setInstanceId(parser.value(instanceIdOption));
  }

  if (parser.isSet(logConsoleOption)) {
      bool enabled = parser.value(logConsoleOption) == "1";
      Daqster::LogManager::instance()->setConsoleEnabled(enabled);
  }

  if (parser.isSet(logLevelOption)) {
      QString levelName = parser.value(logLevelOption);
      Daqster::LogLevel level = Daqster::LogLevel::Warning;
      if (levelName == "Debug") level = Daqster::LogLevel::Debug;
      else if (levelName == "Info") level = Daqster::LogLevel::Info;
      else if (levelName == "Warning") level = Daqster::LogLevel::Warning;
      else if (levelName == "Critical") level = Daqster::LogLevel::Critical;
      else if (levelName == "Fatal") level = Daqster::LogLevel::Fatal;
      Daqster::LogManager::instance()->setConsoleLogLevel(level);
  }

  if (parser.isSet(logRulesOption)) {
      QLoggingCategory::setFilterRules(parser.value(logRulesOption));
  }

  const QStringList args = parser.positionalArguments();

  qCDebug(lcApp) << "Positional Argumments: " << args;

  Daqster::QPluginManager *PluginManager = Daqster::QPluginManager::instance();
  // For correct plugoins shutdown behaviour QPluginManager initialization
  // should be called.
  if (!PluginManager->Initialize()) {
    qCDebug(lcApp) << "QPluginManager Initialization Error";
  }

  qCDebug(lcApp) << "Show window";
  PluginsInit();
  qCDebug(lcApp) << "ARGS: " << args;

  // Define Filter outside if/else scope so it can be used in both sections
  Daqster::PluginFilter Filter;
  Filter.AddFilter(
      PLUGIN_TYPE,
      QString("%1").arg(Daqster::PluginDescription::APPLICATION_PLUGIN));

  QList<Daqster::PluginDescription> PluginsList = PluginManager->GetPluginList(Filter);
  qCDebug(lcApp) << "PluginsList count: " << PluginsList.count();
  int ctr = 0;
  foreach (const Daqster::PluginDescription &Desc, PluginsList) {
    ctr++;
    qCDebug(lcApp) << "  Plugin" << ctr << ": " << Desc.GetProperty(PLUGIN_NAME).toString();
    qCDebug(lcApp) << "  Location" << ctr << ": " << Desc.GetProperty(PLUGIN_LOCATION).toString();

  }

  if (args.count() > 0) {
    if (args.count() > 1) {
      foreach (auto Name, args) {
        // Try multiple approaches for starting the application
        QString executablePath;
        
        // 1. Check if we're in AppImage and AppRun exists
        QString appImageEnv = qgetenv("APPIMAGE");
        if (!appImageEnv.isEmpty()) {
          QString appImagePath = qApp->applicationDirPath() + "/../AppRun";
          if (QFile::exists(appImagePath)) {
            executablePath = appImagePath;
            qCDebug(lcApp) << "Using AppRun script for AppImage environment";
          }
        }
        
        // 2. If no AppRun found, try direct executable
        if (executablePath.isEmpty()) {
          executablePath = "./Daqster";
          qCDebug(lcApp) << "Using direct executable";
        }
        
        ApplicationsManager::Instance().StartApplication(executablePath, QStringList(Name));
        qCDebug(lcApp) << "Start Application: " << Name << " via " << executablePath;
      }
    } else {
      QString input = args[0];
      Daqster::QBasePluginObject *obj = nullptr;
      qCDebug(lcApp) << "\nSearch for plugin: " << input;
      int ctr = 0;
      QString matchedHash;
      QString currentDir = QCoreApplication::applicationDirPath();

      // First pass: try HASH match (toolbar path)
      foreach (const Daqster::PluginDescription &Desc, PluginsList) {
        ctr++;
        qCDebug(lcApp) << "  Plug" << ctr << ": "
                 << Desc.GetProperty(PLUGIN_NAME).toString()
                 << " (" << Desc.GetProperty(PLUGIN_HASH).toString() << ")";
        if (0 == Desc.GetProperty(PLUGIN_HASH).toString().compare(input, Qt::CaseInsensitive)) {
          matchedHash = Desc.GetProperty(PLUGIN_HASH).toString();
          break;
        }
      }

      // Second pass: try NAME match (CLI convenience)
      // Prioritize plugins from current build directory
      if (matchedHash.isEmpty()) {
        QString bestMatch;
        foreach (const Daqster::PluginDescription &Desc, PluginsList) {
          if (0 == Desc.GetProperty(PLUGIN_NAME).toString().compare(input, Qt::CaseInsensitive)) {
            QString location = Desc.GetProperty(PLUGIN_LOCATION).toString();
            // Prefer plugin from current directory
            if (location.startsWith(currentDir)) {
              bestMatch = Desc.GetProperty(PLUGIN_HASH).toString();
              qCDebug(lcApp) << "  Found by name (current dir): " << input << " -> " << bestMatch;
              break;
            }
            // First match if no current dir match found
            if (bestMatch.isEmpty()) {
              bestMatch = Desc.GetProperty(PLUGIN_HASH).toString();
              qCDebug(lcApp) << "  Found by name: " << input << " -> " << bestMatch;
            }
          }
        }
        matchedHash = bestMatch;
      }

      // Create and run the plugin
      if (!matchedHash.isEmpty()) {
        obj = PluginManager->CreatePluginObject(matchedHash, nullptr);
        if (nullptr != obj) {
          qCDebug(lcApp) << "Plugin " << input << " founded! Run it.";
          obj->Initialize();
          QApplication::setApplicationName(matchedHash);
        }
      }
      QConsoleListener *console = new QConsoleListener();
      QObject::connect(
          console, &QConsoleListener::newLine, [&a](const QString &strNewLine) {
            // quit
            if (strNewLine.trimmed().compare("quit", Qt::CaseInsensitive) == 0) {
              qCDebug(lcApp) << "Goodbye";
              a.quit();
            }
          });
    }
    res = a.exec();
  } else {
    qCDebug(lcApp) << __BASE_FILE__ << __FILE__;
    QMainWindow mainWin;
    mainWin.setWindowTitle("Daqster");
    mainWin.resize(400, 60);
    AppToolbar* AppBar = new AppToolbar(&mainWin);
    mainWin.addToolBar(Qt::TopToolBarArea, AppBar);
    mainWin.setCentralWidget(new QWidget(&mainWin));
    mainWin.show();
    res = a.exec();
  }

  Daqster::QPluginManager::instance()->ShutdownPluginManager();
  Daqster::LogManager::instance()->shutdown();
  return res;
}
