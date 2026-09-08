#include <QPluginManager.h>
#include <HeadlessApp.h>
#include <HeadlessEngine.h>
#include <LogManager.h>
#include <LogCategories.h>
#include <ShutdownHandler.h>
#include <daqster_version.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <RuntimeShell.h>

int main(int argc, char *argv[])
{
    // Manual parse for --headless BEFORE QApplication (to set platform)
    bool headlessMode = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--headless" || arg == "-h") {
            headlessMode = true;
            break;
        }
    }

    // For headless: use offscreen platform + HeadlessEngine
    if (headlessMode) {
        Daqster::configureHeadlessPlatform();
    }

    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("NodeRunner");
    QCoreApplication::setApplicationVersion(DAQSTER_VERSION_STRING);

    // Full parser
    QCommandLineParser parser;
    parser.setApplicationDescription("NodeRunner - Run flow files (GUI runtime or headless)");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addOption(QCommandLineOption("headless",
        "Run in headless mode (offscreen, no GUI)"));
    parser.addOption(QCommandLineOption("run", "Run <flow.flow>", "flow"));

    QCommandLineOption logConsoleOption(
        QStringList() << "log-console-enabled",
        "Enable console logging",
        "0|1");
    parser.addOption(logConsoleOption);

    QCommandLineOption logLevelOption(
        QStringList() << "log-level",
        "Minimum console log level",
        "level");
    parser.addOption(logLevelOption);

    QCommandLineOption logRulesOption(
        QStringList() << "log-rules",
        "Qt logging category rules",
        "rules");
    parser.addOption(logRulesOption);

    QCommandLineOption instanceIdOption(
        QStringList() << "instance-id",
        "Child process instance identifier",
        "id");
    parser.addOption(instanceIdOption);

    parser.process(a.arguments());

    // Initialize logging
    Daqster::LogManager::instance()->initialize();

    // Setup shutdown handler
    auto* shutdownHandler = Daqster::ShutdownHandler::create(nullptr);
    shutdownHandler->initialize();
    QObject::connect(shutdownHandler, &Daqster::ShutdownHandler::shutdownRequested, &a, &QCoreApplication::quit);

    if (parser.isSet("instance-id")) {
        Daqster::LogManager::instance()->setInstanceId(parser.value("instance-id"));
    }

    if (parser.isSet("log-console-enabled")) {
        bool enabled = parser.value("log-console-enabled") == "1";
        Daqster::LogManager::instance()->setConsoleEnabled(enabled);
    }

    if (parser.isSet("log-level")) {
        QString levelName = parser.value("log-level");
        Daqster::LogLevel level = Daqster::LogLevel::Warning;
        if (levelName == "Debug") level = Daqster::LogLevel::Debug;
        else if (levelName == "Info") level = Daqster::LogLevel::Info;
        else if (levelName == "Warning") level = Daqster::LogLevel::Warning;
        else if (levelName == "Critical") level = Daqster::LogLevel::Critical;
        else if (levelName == "Fatal") level = Daqster::LogLevel::Fatal;
        Daqster::LogManager::instance()->setConsoleLogLevel(level);
    }

    if (parser.isSet("log-rules")) {
        QLoggingCategory::setFilterRules(parser.value("log-rules"));
    }

    // Initialize plugin manager
    Daqster::QPluginManager* pluginManager = Daqster::QPluginManager::instance();
    if (!pluginManager->Initialize()) {
        qCCritical(lcApp) << "QPluginManager initialization failed";
        return 1;
    }

    pluginManager->SearchForPlugins();

    if (!parser.isSet("run")) {
        qCCritical(lcApp) << "No flow file specified. Use --run <flow.flow>";
        return 1;
    }

    QString flowPath = parser.value("run");
    qCInfo(lcApp) << (parser.isSet("headless") ? "Headless mode" : "GUI Runtime mode") << ": loading flow" << flowPath;

    if (!QFile::exists(flowPath)) {
        qCCritical(lcApp) << "Flow file not found:" << flowPath;
        return 1;
    }

    int result = 0;

    if (parser.isSet("headless")) {
        // HEADLESS MODE: HeadlessEngine
        Daqster::HeadlessEngine engine;
        bool success = engine.loadFlow(flowPath);

        if (!success) {
            qCCritical(lcApp) << "Failed to load flow:" << flowPath;
            return 1;
        }

        qCInfo(lcApp) << "Flow loaded successfully, entering event loop";
        result = a.exec();

        engine.stopAllNodes();
    } else {
        // GUI RUNTIME MODE: RuntimeShell (MDI, autoStart, VideoDisplay)
        RuntimeShell runtime;
        if (!runtime.RunRuntime(flowPath)) {
            qCCritical(lcApp) << "Failed to load flow:" << flowPath;
            return 1;
        }

        qCInfo(lcApp) << "Flow loaded successfully, entering GUI event loop";
        result = a.exec();
    }

    Daqster::QPluginManager::instance()->ShutdownPluginManager();
    Daqster::LogManager::instance()->shutdown();

    return result;
}