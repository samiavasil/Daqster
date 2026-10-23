#include "AppBootstrap.h"
#include "ApplicationsManager.h"
#include "QPluginManager.h"
#include "LogCategories.h"
#include "daqster_version.h"

#include <AppToolbar.h>
#include <QApplication>
#include <QBasePluginObject.h>
#include <QCommandLineOption>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QMainWindow>
#include <QMessageBox>
#include <QMetaObject>
#include <QSettings>
#include "QConsoleListener.h"
#include "main.h"

// __BASE_FILE__ is a GCC/Clang predefined macro (the basename of the file
// being compiled). MSVC does not define it, so fall back to __FILE__ there.
#if defined(_MSC_VER)
#define DAQSTER_BASE_FILE __FILE__
#else
#define DAQSTER_BASE_FILE __BASE_FILE__
#endif

// Create a plugin object for every enabled plugin. The objects are intentionally
// NOT deleted here: capability interfaces (INodeProvider / IWidgetProvider /
// IRuntimeHost, REQ-SW-PL-051) hand out pointers to their plugin objects, and
// those pointers are cached for the whole session — the node editor keeps its
// IWidgetProvider in ChatGraphModel. Destroying the objects at the first
// event-loop turn left that pointer dangling and every node placement
// segfaulted. PluginRegistry::shutdownAll() disposes of them on exit.
//
// Plugin discovery itself (QPluginManager::Initialize() + SearchForPlugins())
// is done by Daqster::AppBootstrap::initializePlugins() before this runs.
void PluginsInit() {
  Daqster::QPluginManager *PluginManager = Daqster::QPluginManager::instance();

  if (nullptr != PluginManager) {
    qCDebug(lcApp) << "Plugin Manager: " << PluginManager;
    QList<Daqster::PluginDescription> PluginsList =
        PluginManager->GetPluginList();
    foreach (const Daqster::PluginDescription &Desc, PluginsList) {
      if (!Desc.IsEnabled()) continue;
      Daqster::QBasePluginObject* obj = PluginManager->CreatePluginObject(Desc.GetProperty(PLUGIN_HASH).toString(), nullptr);
      Q_UNUSED(obj)
    }
  }
}

int main(int argc, char *argv[]) {
  int res = 0;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
  QApplication a(argc, argv);
  QApplication::setApplicationName("Daqster");
  QApplication::setApplicationVersion(DAQSTER_VERSION_STRING);

  // ── Shared, widget-free startup (REQ-SW-PL-048) ──────────────────────────
  Daqster::AppBootstrap boot(
      a, "Daqster - Plugin Application Launcher & Node Editor");
  boot.addStandardOptions();

  boot.parser().addPositionalArgument(
      "apps ...",
      QCoreApplication::translate("main",
                                  "One or more Application plugin names to start automatically"));

  QCommandLineOption targetDirectoryOption(
      QStringList() << "t" << "target-directory",
      QCoreApplication::translate("main", "Copy all source files into <directory>."),
      QCoreApplication::translate("main", "directory"));
  boot.parser().addOption(targetDirectoryOption);

  // Parses the shared --log-* / --instance-id options and applies them.
  boot.processArguments();

  // Load theme if configured (from QSettings or DAQSTER_THEME env var passed by parent toolbar)
  QSettings appSettings("Daqster", "Daqster");
  QString theme = appSettings.value("Theme/Style", "default").toString();
  QString envTheme = qgetenv("DAQSTER_THEME");
  if (!envTheme.isEmpty()) {
      theme = envTheme;
      qCDebug(lcApp) << "Theme from DAQSTER_THEME env:" << theme;
  } else {
      qCDebug(lcApp) << "Theme from QSettings:" << theme;
  }
  if (theme == "dark") {
      qCDebug(lcApp) << "Loading dark stylesheet";
      QFile styleFile(":/toolbar/icons/StyleFile");
      if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
          a.setStyleSheet(styleFile.readAll());
          styleFile.close();
          qCDebug(lcApp) << "Dark stylesheet applied";
      } else {
          qCDebug(lcApp) << "Failed to open StyleFile resource";
      }
  }

  const QStringList args = boot.parser().positionalArguments();

  qCDebug(lcApp) << "Positional Arguments: " << args;

  boot.installShutdownHandler();

  if (!boot.initializePlugins()) {
    qCDebug(lcApp) << "QPluginManager Initialization Error";
  }

  qCDebug(lcApp) << "Show window";
  PluginsInit();
  qCDebug(lcApp) << "ARGS: " << args;

  Daqster::QPluginManager* PluginManager = Daqster::QPluginManager::instance();

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

  // Console listener: stdin "quit" handler
  QConsoleListener* console = new QConsoleListener();
  QObject::connect(
      console, &QConsoleListener::newLine, [&a](const QString& strNewLine) {
        if (strNewLine.trimmed().compare("quit", Qt::CaseInsensitive) == 0) {
          qCDebug(lcApp) << "Goodbye";
          a.quit();
        }
      });

  if (args.count() > 0) {
    if (args.count() > 1) {
      foreach (auto Name, args) {
        QString executablePath = "./Daqster";
        ApplicationsManager::Instance().StartApplication(executablePath, QStringList(Name));
        qCDebug(lcApp) << "Start Application: " << Name << " via " << executablePath;
      }
    } else {
      QString input = args[0];
      Daqster::QBasePluginObject* obj = nullptr;
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
      if (matchedHash.isEmpty()) {
        QString bestMatch;
        foreach (const Daqster::PluginDescription &Desc, PluginsList) {
          if (0 == Desc.GetProperty(PLUGIN_NAME).toString().compare(input, Qt::CaseInsensitive)) {
            QString location = Desc.GetProperty(PLUGIN_LOCATION).toString();
            if (location.startsWith(currentDir)) {
              bestMatch = Desc.GetProperty(PLUGIN_HASH).toString();
              qCDebug(lcApp) << "  Found by name (current dir): " << input << " -> " << bestMatch;
              break;
            }
            if (bestMatch.isEmpty()) {
              bestMatch = Desc.GetProperty(PLUGIN_HASH).toString();
              qCDebug(lcApp) << "  Found by name: " << input << " -> " << bestMatch;
            }
          }
        }
        matchedHash = bestMatch;
      }

      if (!matchedHash.isEmpty()) {
        obj = PluginManager->CreatePluginObject(matchedHash, nullptr);
        if (nullptr != obj) {
          qCDebug(lcApp) << "Plugin " << input << " founded! Run it.";
          obj->Initialize();
          QApplication::setApplicationName(matchedHash);
        } else {
          qCCritical(lcApp) << "Plugin " << input << " (hash " << matchedHash
                            << ") found but failed to create plugin object";
          QMessageBox::critical(
              nullptr, "Daqster",
              QString("Application plugin \"%1\" was found but failed to load.")
                  .arg(input));
          boot.shutdown();
          return 1;
        }
      } else {
        qCCritical(lcApp) << "Plugin " << input << " not found";
        QMessageBox::critical(
            nullptr, "Daqster",
            QString("Application plugin \"%1\" was not found. Rebuild the "
                    "plugin or launch the Daqster main window and use the "
                    "toolbar.")
                .arg(input));
        boot.shutdown();
        return 1;
      }
    }
    res = a.exec();
  } else {
    qCDebug(lcApp) << DAQSTER_BASE_FILE << __FILE__;
    QMainWindow mainWin;
    mainWin.setWindowTitle("Daqster");
    mainWin.resize(400, 60);
    AppToolbar* AppBar = new AppToolbar(&mainWin);
    mainWin.addToolBar(Qt::TopToolBarArea, AppBar);
    mainWin.setCentralWidget(new QWidget(&mainWin));
    mainWin.show();
    res = a.exec();
  }

  boot.shutdown();
  return res;
}
