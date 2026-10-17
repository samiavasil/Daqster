#pragma once

#include "framework_core_export.h"

#include <capabilities/IRuntimeHost.h>

#include <QCommandLineParser>
#include <QString>

class QCoreApplication;

namespace Daqster {

/**
 * @brief Widget-free shared application bootstrap for Daqster executables.
 *
 * NodeRunner (and, before this, Daqster) duplicated the same startup
 * choreography:
 *
 *   logging init -> ShutdownHandler -> QPluginManager::Initialize()/Search
 *   -> runtime-host resolution -> event loop -> ordered teardown
 *
 * AppBootstrap owns that sequence once, in FrameworkCore. FrameworkCore links
 * only QtCore (plus QtNodesCore), so this class is deliberately widget-free: it
 * does NOT construct the application object. The caller decides between
 * QCoreApplication / QGuiApplication / QApplication, constructs it, and passes
 * it in. That keeps the bootstrap usable from a headless binary that must not
 * pull in QtWidgets, while still serving a GUI binary that passes a
 * QApplication.
 *
 * What stays in each main() (app-specific, not shared):
 *   - the application object itself and its type,
 *   - setApplicationName() / setApplicationVersion(),
 *   - app-specific CLI options, positional arguments and help text,
 *   - app-specific UI / launch behaviour.
 *
 * Typical use:
 * @code
 *   const bool headless = AppBootstrap::detectHeadless(argc, argv);
 *   AppBootstrap::preparePlatform(headless);          // before the app exists
 *   QApplication app(argc, argv);
 *   QCoreApplication::setApplicationName(...);
 *
 *   AppBootstrap boot(app, "NodeRunner - run a .flow scene");
 *   boot.addStandardOptions();
 *   boot.parser().addOption(...);                     // app-specific options
 *   boot.processArguments();
 *   boot.installShutdownHandler();
 *   if (!boot.initializePlugins()) return 1;
 *   IRuntimeHost* host = boot.resolveRuntimeHost(mode);
 *   if (!boot.prepareHost(host)) return 1;
 *   if (!boot.loadFlow(host, path)) return 1;
 *   const int rc = boot.exec();
 *   boot.shutdown();
 *   return rc;
 * @endcode
 *
 * REQ-SW-PL-048 (shared runtime startup) / REQ-SW-PL-054 (REST control plane).
 */
class FRAMEWORK_CORE_EXPORT AppBootstrap
{
public:
    // ── Pre-application helpers ────────────────────────────────────────────
    // Platform selection happens during application construction, so these
    // must run before the Q*Application object is built.

    /**
     * @brief Detect `--headless` in the raw argv.
     *
     * Runs before QCommandLineParser exists (and before the platform plugin is
     * chosen), so a tiny hand-rolled scan is used on purpose.
     */
    static bool detectHeadless(int argc, char** argv);

    /**
     * @brief Select the Qt platform plugin for the requested mode.
     *
     * Headless selects the "offscreen" platform (honouring an explicit
     * QT_QPA_PLATFORM in the environment); GUI mode changes nothing.
     */
    static void preparePlatform(bool headless);

    // ── Lifecycle ──────────────────────────────────────────────────────────

    AppBootstrap(QCoreApplication& app, QString applicationDescription);
    ~AppBootstrap();

    AppBootstrap(const AppBootstrap&) = delete;
    AppBootstrap& operator=(const AppBootstrap&) = delete;

    // ── Command line ───────────────────────────────────────────────────────
    /// Adds help/version, the shared `--log-*` / `--instance-id` options and
    /// the `--rest-*` control-plane options. Call before adding app options.
    void addStandardOptions();

    /// The shared parser. Apps register their own options here.
    QCommandLineParser& parser() { return m_parser; }

    /// parser.process() + apply the logging (and control-plane) options.
    void processArguments();

    // ── Plugin + host lifecycle ────────────────────────────────────────────

    /// QPluginManager::Initialize() followed by SearchForPlugins().
    ///
    /// @p mode gates plugin loading (REQ-SW-PL-073): in Headless, plugins that
    /// declare `PluginType: "gui"` are not loaded, so the widget stack never
    /// enters a headless process. Defaults to Gui (load everything).
    bool initializePlugins(RuntimeMode mode = RuntimeMode::Gui);

    /// Installs the platform ShutdownHandler and wires it to quit the app.
    void installShutdownHandler();

    /// Resolves the single IRuntimeHost that provides @p mode (nullptr + logged
    /// reason otherwise, or when more than one plugin claims the mode).
    IRuntimeHost* resolveRuntimeHost(RuntimeMode mode);

    /// Remembers @p host and, if `--rest-port` was given, asks it to expose the
    /// control plane. Returns false when the control plane was requested but the
    /// host does not support it (fail loudly — never silently skip REST).
    bool prepareHost(IRuntimeHost* host);

    /// Loads a .flow scene through @p host.
    bool loadFlow(IRuntimeHost* host, const QString& flowPath);

    /// Runs the application event loop.
    int exec();

    /// Ordered teardown: stop nodes, shut the plugin manager down, close the
    /// LogManager. Safe to call after exec(); the node stop is guarded so the
    /// aboutToQuit hook and this call never both fire it.
    void shutdown();

    // ── Accessors ──────────────────────────────────────────────────────────
    const ControlPlaneOptions& controlPlaneOptions() const { return m_control; }
    QCoreApplication& application() { return m_app; }

private:
    void applyStandardOptionValues();

    QCoreApplication& m_app;
    QCommandLineParser m_parser;
    QString m_description;
    ControlPlaneOptions m_control;

    IRuntimeHost* m_host = nullptr;
    bool m_nodesStopped = false;
    bool m_shutdownHandlerInstalled = false;
};

} // namespace Daqster
