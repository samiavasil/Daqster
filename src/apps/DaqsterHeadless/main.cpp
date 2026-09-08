#include <QPluginManager.h>
#include <HeadlessEngine.h>
#include <LogManager.h>
#include <LogCategories.h>
#include <ShutdownHandler.h>
#include <daqster_version.h>

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("DaqsterHeadless");
    QCoreApplication::setApplicationVersion(DAQSTER_VERSION_STRING);

    // Initialize logging
    Daqster::LogManager::instance()->initialize();

    // Setup shutdown handler
    auto* shutdownHandler = Daqster::ShutdownHandler::create(&a);
    shutdownHandler->initialize();
    QObject::connect(shutdownHandler, &Daqster::ShutdownHandler::shutdownRequested, &a, &QCoreApplication::quit);

    QCommandLineParser parser;
    parser.setApplicationDescription("Daqster Headless - Run flow files without GUI");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption runOption(
        QStringList() << "run",
        QCoreApplication::translate("main", "Run <flow.flow> in headless mode"),
        QCoreApplication::translate("main", "flow"));
    parser.addOption(runOption);

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

    // Initialize plugin manager
    Daqster::QPluginManager* pluginManager = Daqster::QPluginManager::instance();
    if (!pluginManager->Initialize()) {
        qCCritical(lcApp) << "QPluginManager initialization failed";
        return 1;
    }

    pluginManager->SearchForPlugins();

    if (!parser.isSet(runOption)) {
        qCCritical(lcApp) << "No flow file specified. Use --run <flow.flow>";
        parser.showHelp(1);
        return 1;
    }

    QString flowPath = parser.value(runOption);
    qCInfo(lcApp) << "Headless mode: loading flow" << flowPath;

    if (!QFile::exists(flowPath)) {
        qCCritical(lcApp) << "Flow file not found:" << flowPath;
        return 1;
    }

    // Create headless engine
    Daqster::HeadlessEngine engine;
    bool success = engine.loadFlow(flowPath);

    if (!success) {
        qCCritical(lcApp) << "Failed to load flow:" << flowPath;
        return 1;
    }

    qCInfo(lcApp) << "Flow loaded successfully, entering event loop";

    int result = a.exec();

    // Cleanup
    engine.stopAllNodes();
    Daqster::QPluginManager::instance()->ShutdownPluginManager();
    Daqster::LogManager::instance()->shutdown();

    return result;
}