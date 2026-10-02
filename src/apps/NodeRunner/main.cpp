/**
 * NodeRunner — single entry point for running .flow scenes (REQ-SW-PL-053).
 *
 * The binary deliberately knows nothing about any concrete plugin: it links
 * only FrameworkCore and asks the plugin manager which loaded plugin can host
 * the requested runtime mode.
 *
 *   NodeRunner --run <flow.flow>            -> RuntimeMode::Gui
 *                                             FrameworkGuiPlugin  (RuntimeShell)
 *   NodeRunner --headless --run <flow.flow>  -> RuntimeMode::Headless
 *                                             FrameworkCorePlugin (HeadlessEngine)
 *
 * Because no link-time dependency on FrameworkGuiPlugin exists, the mode is a
 * pure runtime decision: the GUI plugin is dlopen()ed only when asked for.
 *
 * QtWidgets is still on the link line, because the GUI mode needs
 * QApplication and in Qt5 QApplication itself lives in QtWidgets. Dropping it
 * requires node models to stop building widgets in their constructors — see
 * REQ-SW-PL-053 AC6.
 *
 * See Daqster::IRuntimeHost (src/plugins/common/capabilities/IRuntimeHost.h).
 */

#include <QPluginManager.h>
#include <HeadlessApp.h>
#include <LogManager.h>
#include <LogCategories.h>
#include <ShutdownHandler.h>
#include <daqster_version.h>
#include <capabilities/IRuntimeHost.h>

// QApplication rather than QGuiApplication: node models still build their
// widgets in their constructors, so the first node instantiation aborts with
// "QWidget: Cannot create a QWidget without QApplication". Switching to
// QGuiApplication is what eventually drops QtWidgets from the link line, and
// it depends on the per-node core/gui split (REQ-SW-PL-051 follow-up,
// REQ-SW-PL-053 AC 7).
#include <QApplication>
using DaqsterRunnerApp = QApplication;

#include <QCommandLineParser>
#include <QFile>
#include <QStringList>

namespace {

/**
 * @brief Resolve the runtime host for `mode` from the loaded plugins.
 *
 * Returns nullptr and reports the reason when no plugin provides that mode.
 */
Daqster::IRuntimeHost* resolveHost(Daqster::RuntimeMode mode)
{
    Daqster::QPluginManager* pluginManager = Daqster::QPluginManager::instance();
    const QList<Daqster::IRuntimeHost*> hosts = pluginManager->runtimeHosts(mode);

    if (hosts.isEmpty()) {
        qCCritical(lcApp) << "No loaded plugin provides the"
                          << (mode == Daqster::RuntimeMode::Headless ? "headless" : "GUI")
                          << "runtime host. Check that the matching framework plugin is"
                          << "installed and enabled.";
        return nullptr;
    }

    if (hosts.size() > 1) {
        // Ambiguity would mean the wrong engine could win; fail loudly instead
        // of silently picking the first one.
        qCCritical(lcApp) << "Multiple plugins provide the same runtime host:"
                          << hosts.size() << "found, expected exactly one.";
        return nullptr;
    }

    return hosts.first();
}

} // namespace

int main(int argc, char *argv[])
{
    // ── Platform selection must happen before the app object is built ───────
    // The platform plugin is read at construction, so --headless is parsed by
    // hand here instead of via QCommandLineParser.
    bool headlessMode = false;
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == "--headless") {
            headlessMode = true;
            break;
        }
    }

    if (headlessMode) {
        Daqster::configureHeadlessPlatform();
    }

    QGuiApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    DaqsterRunnerApp app(argc, argv);
    QCoreApplication::setApplicationName(DAQSTER_RUNNER_TARGET_NAME);
    QCoreApplication::setApplicationVersion(DAQSTER_VERSION_STRING);

    // ── Command line ───────────────────────────────────────────────────────
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "NodeRunner - Run a .flow scene, headless or with a visible runtime UI");
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption headlessOption(
        QStringList() << "headless",
        "Run without a visible UI (offscreen platform, no display required)");
    parser.addOption(headlessOption);

    const QCommandLineOption runOption(
        QStringList() << "run",
        "Run <flow.flow>",
        "flow");
    parser.addOption(runOption);

    const QCommandLineOption logConsoleOption(
        QStringList() << "log-console-enabled",
        "Enable console logging",
        "0|1");
    parser.addOption(logConsoleOption);

    const QCommandLineOption logLevelOption(
        QStringList() << "log-level",
        "Minimum console log level (Debug|Info|Warning|Critical|Fatal)",
        "level");
    parser.addOption(logLevelOption);

    const QCommandLineOption logRulesOption(
        QStringList() << "log-rules",
        "Qt logging category rules",
        "rules");
    parser.addOption(logRulesOption);

    const QCommandLineOption instanceIdOption(
        QStringList() << "instance-id",
        "Child process instance identifier",
        "id");
    parser.addOption(instanceIdOption);

    parser.process(app.arguments());

    if (!parser.isSet("run")) {
        qCCritical(lcApp) << "No flow file specified. Use --run <flow.flow>";
        return 1;
    }

    const QString flowPath = parser.value("run");
    if (!QFile::exists(flowPath)) {
        qCCritical(lcApp) << "Flow file not found:" << flowPath;
        return 1;
    }

    // ── Logging ────────────────────────────────────────────────────────────
    Daqster::LogManager::instance()->initialize();

    if (parser.isSet("instance-id")) {
        Daqster::LogManager::instance()->setInstanceId(parser.value("instance-id"));
    }
    if (parser.isSet("log-console-enabled")) {
        Daqster::LogManager::instance()->setConsoleEnabled(parser.value("log-console-enabled") == "1");
    }
    if (parser.isSet("log-level")) {
        const QString levelName = parser.value("log-level");
        Daqster::LogLevel level = Daqster::LogLevel::Warning;
        if (levelName == "Debug") level = Daqster::LogLevel::Debug;
        else if (levelName == "Info") level = Daqster::LogLevel::Info;
        else if (levelName == "Critical") level = Daqster::LogLevel::Critical;
        else if (levelName == "Fatal") level = Daqster::LogLevel::Fatal;
        Daqster::LogManager::instance()->setConsoleLogLevel(level);
    }
    if (parser.isSet("log-rules")) {
        QLoggingCategory::setFilterRules(parser.value("log-rules"));
    }

    // ── Node shutdown ordering ─────────────────────────────────────────────
    // QPluginManager::Initialize() connects ShutdownPluginManager() to
    // aboutToQuit, so the plugin objects — and the engine each one owns — are
    // destroyed *before* app.exec() returns. Stopping the nodes from after
    // exec() would therefore call through a dangling pointer, so the stop is
    // registered here, ahead of that connection, and guarded to run once.
    Daqster::IRuntimeHost* host = nullptr;
    bool nodesStopped = false;
    const auto stopNodes = [&host, &nodesStopped]() {
        if (host && !nodesStopped) {
            nodesStopped = true;
            host->stopAllNodes();
        }
    };
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, stopNodes);

    // ── Shutdown handling ──────────────────────────────────────────────────
    auto* shutdownHandler = Daqster::ShutdownHandler::create(nullptr);
    shutdownHandler->initialize();
    QObject::connect(shutdownHandler, &Daqster::ShutdownHandler::shutdownRequested,
                     &app, &QCoreApplication::quit);

    // ── Plugins ────────────────────────────────────────────────────────────
    // The engine for the requested mode comes from whichever framework plugin
    // is installed; nothing below this line differs between headless and GUI.
    Daqster::QPluginManager* pluginManager = Daqster::QPluginManager::instance();
    if (!pluginManager->Initialize()) {
        qCCritical(lcApp) << "QPluginManager initialization failed";
        return 1;
    }
    pluginManager->SearchForPlugins();

    const Daqster::RuntimeMode mode = headlessMode ? Daqster::RuntimeMode::Headless
                                                   : Daqster::RuntimeMode::Gui;
    qCInfo(lcApp) << "Runtime mode:" << (headlessMode ? "headless" : "GUI")
                  << "loading flow" << flowPath;

    host = resolveHost(mode);
    if (!host) {
        return 1;
    }

    if (!host->loadFlow(flowPath)) {
        qCCritical(lcApp) << "Failed to load flow:" << flowPath;
        return 1;
    }

    qCInfo(lcApp) << "Flow loaded successfully, entering event loop";
    const int result = app.exec();

    // Normally already done via aboutToQuit; the guard makes this a no-op then.
    // It still matters if the event loop ends for any other reason.
    stopNodes();

    pluginManager->ShutdownPluginManager();
    Daqster::LogManager::instance()->shutdown();

    return result;
}
