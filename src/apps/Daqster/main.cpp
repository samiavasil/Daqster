#include "ApplicationsManager.h"
#include "QPluginManager.h"

#include <AppToolbar.h>
#include <QApplication>
#include <QBasePluginObject.h>
#include <QCommandLineParser>
#include <QFile>
#include "debug.h"
#include "QConsoleListener.h"
#include "main.h"

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
    //  PluginManager->SearchForPlugins();
    // PluginManager->ShowPluginManagerGui();
    QList<Daqster::PluginDescription> PluginsList =
        PluginManager->GetPluginList();
    /*Just try to load/unload all plugins in initialization phase*/
    foreach (const Daqster::PluginDescription &Desc, PluginsList) {
      for (int i = 0; i < 1; i++) {
        PluginManager
            ->CreatePluginObject(Desc.GetProperty(PLUGIN_HASH).toString(),
                                 nullptr)
            ->deleteLater();
      }
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
  }

  DEBUG << "Show window";
  PluginsInit();
  qDebug() << "ARGS: " << args;
  
  // Define Filter outside if/else scope so it can be used in both sections
  Daqster::PluginFilter Filter;
  Filter.AddFilter(
      PLUGIN_TYPE,
      QString("%1").arg(Daqster::PluginDescription::APPLICATION_PLUGIN));
  
  if (args.count() > 0) {
    QList<Daqster::PluginDescription> PluginsList =
        PluginManager->GetPluginList(Filter);

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
      QString Name = args[0];
      Daqster::QBasePluginObject *obj = nullptr;
      qDebug() << "\nSearch for plugin: " << Name;
      
      // Use the same filter as in the if section
      QList<Daqster::PluginDescription> PluginsList = PluginManager->GetPluginList(Filter);
      qDebug() << "PluginsList count: " << PluginsList.count();

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
      QConsoleListener *console = new QConsoleListener();
      QObject::connect(
          console, &QConsoleListener::newLine, [&a](const QString &strNewLine) {
            static int aa;
            qDebug() << "Echo :" << aa++ << strNewLine;
            // quit
            if (strNewLine.trimmed().compare("quit", Qt::CaseInsensitive) == 0) {
              qDebug() << "Goodbye";
              a.quit();
            }
          });
    }
    res = a.exec();
  } else {

    // MainWindow w;
    // w.show();
    qDebug() << __BASE_FILE__ << __FILE__;
    AppToolbar AppBar;
    AppBar.show();
    res = a.exec();
  }

  // Daqster::QPluginManager::instance()->ShutdownPluginManager();
  return res;
}
