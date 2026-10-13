#include "AppBootstrap.h"

#include <HeadlessApp.h>
#include <LogCategories.h>
#include <LogManager.h>
#include <QPluginManager.h>
#include <ShutdownHandler.h>

#include <QCommandLineOption>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QStringList>

#include <utility>

namespace Daqster {

namespace {

/// Small helper: `QStringList() << name` reads better than an initializer list
/// here and is unambiguous across Qt5/Qt6.
QStringList optionNames(const char* name)
{
    return QStringList() << QString::fromLatin1(name);
}

} // namespace

// ── Pre-application helpers ─────────────────────────────────────────────────

bool AppBootstrap::detectHeadless(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String("--headless"))
            return true;
    }
    return false;
}

void AppBootstrap::preparePlatform(bool headless)
{
    if (headless)
        configureHeadlessPlatform();
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

AppBootstrap::AppBootstrap(QCoreApplication& app, QString applicationDescription)
    : m_app(app)
    , m_description(std::move(applicationDescription))
{
    // Register the node-stop hook immediately: QPluginManager::Initialize()
    // connects ShutdownPluginManager() to aboutToQuit, and that connection must
    // run *after* the nodes are stopped while their graph is still valid.
    // Registering here (before initializePlugins()) guarantees the order.
    QObject::connect(&m_app, &QCoreApplication::aboutToQuit, &m_app, [this]() {
        if (m_host && !m_nodesStopped) {
            m_nodesStopped = true;
            m_host->stopAllNodes();
        }
    });
}

AppBootstrap::~AppBootstrap() = default;

// ── Command line ────────────────────────────────────────────────────────────

void AppBootstrap::addStandardOptions()
{
    m_parser.setApplicationDescription(m_description);
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    m_parser.addOption(QCommandLineOption(
        optionNames("log-console-enabled"), "Enable console logging", "0|1"));
    m_parser.addOption(QCommandLineOption(
        optionNames("log-level"),
        "Minimum console log level (Debug|Info|Warning|Critical|Fatal)", "level"));
    m_parser.addOption(QCommandLineOption(
        optionNames("log-rules"), "Qt logging category rules", "rules"));
    m_parser.addOption(QCommandLineOption(
        optionNames("instance-id"), "Child process instance identifier", "id"));

    // REST / WebSocket control plane (REQ-SW-PL-054). Disabled unless
    // --rest-port is given explicitly; loopback bind by default.
    m_parser.addOption(QCommandLineOption(
        optionNames("rest-port"),
        "Enable the REST/WebSocket control plane on <port> (0 = disabled)",
        "port"));
    m_parser.addOption(QCommandLineOption(
        optionNames("rest-bind"),
        "Control-plane bind address (default 127.0.0.1)", "address"));
    m_parser.addOption(QCommandLineOption(
        optionNames("rest-token"),
        "Bearer token required by the control plane", "token"));
    m_parser.addOption(QCommandLineOption(
        optionNames("rest-web-root"),
        "Directory with the control web UI to serve", "dir"));
}

void AppBootstrap::processArguments()
{
    m_parser.process(m_app);
    applyStandardOptionValues();
}

void AppBootstrap::applyStandardOptionValues()
{
    LogManager* log = LogManager::instance();
    log->initialize();

    if (m_parser.isSet("instance-id"))
        log->setInstanceId(m_parser.value("instance-id"));

    if (m_parser.isSet("log-console-enabled"))
        log->setConsoleEnabled(m_parser.value("log-console-enabled") == QLatin1String("1"));

    if (m_parser.isSet("log-level")) {
        const QString levelName = m_parser.value("log-level");
        LogLevel level = LogLevel::Warning;
        if (levelName == QLatin1String("Debug"))         level = LogLevel::Debug;
        else if (levelName == QLatin1String("Info"))     level = LogLevel::Info;
        else if (levelName == QLatin1String("Warning"))  level = LogLevel::Warning;
        else if (levelName == QLatin1String("Critical")) level = LogLevel::Critical;
        else if (levelName == QLatin1String("Fatal"))    level = LogLevel::Fatal;
        log->setConsoleLogLevel(level);
    }

    if (m_parser.isSet("log-rules"))
        QLoggingCategory::setFilterRules(m_parser.value("log-rules"));

    if (m_parser.isSet("rest-port")) {
        bool ok = false;
        const uint port = m_parser.value("rest-port").toUInt(&ok);
        if (ok && port == 0) {
            m_control.enabled = false;
        } else if (ok && port <= 65535) {
            m_control.enabled = true;
            m_control.port = static_cast<quint16>(port);
        } else {
            qCWarning(lcApp) << "Ignoring invalid --rest-port"
                             << m_parser.value("rest-port");
        }
    }
    if (m_parser.isSet("rest-bind"))
        m_control.bindAddress = m_parser.value("rest-bind");
    if (m_parser.isSet("rest-token"))
        m_control.authToken = m_parser.value("rest-token");
    if (m_parser.isSet("rest-web-root"))
        m_control.webRoot = m_parser.value("rest-web-root");
}

// ── Plugin + host lifecycle ─────────────────────────────────────────────────

bool AppBootstrap::initializePlugins()
{
    QPluginManager* pluginManager = QPluginManager::instance();
    if (!pluginManager->Initialize()) {
        qCCritical(lcApp) << "QPluginManager initialization failed";
        return false;
    }
    pluginManager->SearchForPlugins();
    return true;
}

void AppBootstrap::installShutdownHandler()
{
    if (m_shutdownHandlerInstalled)
        return;

    ShutdownHandler* shutdownHandler = ShutdownHandler::create(&m_app);
    shutdownHandler->initialize();
    QObject::connect(shutdownHandler, &ShutdownHandler::shutdownRequested,
                     &m_app, &QCoreApplication::quit);
    m_shutdownHandlerInstalled = true;
}

IRuntimeHost* AppBootstrap::resolveRuntimeHost(RuntimeMode mode)
{
    QPluginManager* pluginManager = QPluginManager::instance();
    const QList<IRuntimeHost*> hosts = pluginManager->runtimeHosts(mode);

    if (hosts.isEmpty()) {
        qCCritical(lcApp) << "No loaded plugin provides the"
                          << (mode == RuntimeMode::Headless ? "headless" : "GUI")
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

bool AppBootstrap::prepareHost(IRuntimeHost* host)
{
    m_host = host;

    if (!m_control.enabled)
        return true;

    if (!host->configureControlPlane(m_control)) {
        qCCritical(lcApp) << "REST control plane requested on"
                          << m_control.bindAddress << ":" << m_control.port
                          << "but the selected runtime host did not enable it"
                          << "(unsupported, or rejected options — see the log above).";
        return false;
    }

    qCInfo(lcApp) << "REST control plane requested on"
                  << m_control.bindAddress << ":" << m_control.port;
    return true;
}

bool AppBootstrap::loadFlow(IRuntimeHost* host, const QString& flowPath)
{
    return host->loadFlow(flowPath);
}

int AppBootstrap::exec()
{
    return m_app.exec();
}

void AppBootstrap::shutdown()
{
    // Normally already stopped via the aboutToQuit hook; the guard makes this a
    // no-op then. It still matters if the event loop ended for another reason.
    if (m_host && !m_nodesStopped) {
        m_nodesStopped = true;
        m_host->stopAllNodes();
    }

    if (QPluginManager* pluginManager = QPluginManager::instance())
        pluginManager->ShutdownPluginManager();

    LogManager::instance()->shutdown();
}

} // namespace Daqster
