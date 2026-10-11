/**
 * NodeRunner — single entry point for running .flow scenes (REQ-SW-PL-053).
 *
 * The binary deliberately knows nothing about any concrete plugin: it links
 * only FrameworkCore and asks the plugin manager which loaded plugin can host
 * the requested runtime mode.
 *
 *   NodeRunner --run <flow.flow>            -> RuntimeMode::Gui
 *                                             RuntimeHostGuiPlugin  (RuntimeShell)
 *   NodeRunner --headless --run <flow.flow>  -> RuntimeMode::Headless
 *                                             RuntimeHostHeadlessPlugin (HeadlessEngine)
 *
 * Because no link-time dependency on RuntimeHostGuiPlugin exists, the mode is a
 * pure runtime decision: the GUI plugin is dlopen()ed only when asked for.
 *
 * The shared startup choreography (platform selection, logging, ShutdownHandler,
 * plugin init, host resolution, control-plane wiring, teardown) now lives in
 * Daqster::AppBootstrap (FrameworkCore, widget-free) — see REQ-SW-PL-048. This
 * file only keeps the runner-specific bits: application type, name/version,
 * the --headless/--run options and the flow path validation.
 *
 * The application type follows the mode (REQ-SW-PL-056): GUI mode builds a
 * QApplication, headless mode a QGuiApplication. After the core/gui split
 * (REQ-SW-PL-051) no node model constructs widgets on the headless path, so
 * QGuiApplication would be sufficient there — but this single dual-mode binary
 * still uses QApplication for both modes (in Qt5 QApplication itself lives in
 * QtWidgets), and Qt::Widgets stays on the link line for the GUI branch. The
 * long-term goal (goal B) is a separate headless binary; see REQ-SW-PL-056 AC5.
 *
 * See Daqster::IRuntimeHost (src/plugins/common/capabilities/IRuntimeHost.h).
 */

#include <AppBootstrap.h>

#include <LogCategories.h>
#include <daqster_version.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCoreApplication>
#include <QFile>
#include <QStringList>

int main(int argc, char *argv[])
{
    // ── Platform selection must happen before the app object is built ───────
    // The platform plugin is read at construction, so --headless is parsed from
    // the raw argv by the bootstrap instead of via QCommandLineParser.
    const bool headlessMode = Daqster::AppBootstrap::detectHeadless(argc, argv);
    Daqster::AppBootstrap::preparePlatform(headlessMode);

    QGuiApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(DAQSTER_RUNNER_TARGET_NAME);
    QCoreApplication::setApplicationVersion(DAQSTER_VERSION_STRING);

    Daqster::AppBootstrap boot(
        app, "NodeRunner - Run a .flow scene, headless or with a visible runtime UI");

    // ── Command line: shared options + runner-specific ones ────────────────
    boot.addStandardOptions();

    const QCommandLineOption headlessOption(
        QStringList() << "headless",
        "Run without a visible UI (offscreen platform, no widget constructed)");
    boot.parser().addOption(headlessOption);

    const QCommandLineOption runOption(
        QStringList() << "run",
        "Run <flow.flow>",
        "flow");
    boot.parser().addOption(runOption);

    boot.processArguments();

    if (!boot.parser().isSet("run")) {
        qCCritical(lcApp) << "No flow file specified. Use --run <flow.flow>";
        return 1;
    }

    const QString flowPath = boot.parser().value("run");
    if (!QFile::exists(flowPath)) {
        qCCritical(lcApp) << "Flow file not found:" << flowPath;
        return 1;
    }

    // ── Shutdown handling + plugins ────────────────────────────────────────
    boot.installShutdownHandler();

    if (!boot.initializePlugins())
        return 1;

    const Daqster::RuntimeMode mode = headlessMode ? Daqster::RuntimeMode::Headless
                                                   : Daqster::RuntimeMode::Gui;
    qCInfo(lcApp) << "Runtime mode:" << (headlessMode ? "headless" : "GUI")
                  << "loading flow" << flowPath;

    Daqster::IRuntimeHost* host = boot.resolveRuntimeHost(mode);
    if (!host)
        return 1;

    // Wires the optional REST control plane (--rest-*) to the host before the
    // flow starts; a requested-but-unsupported control plane fails loudly here.
    if (!boot.prepareHost(host))
        return 1;

    if (!boot.loadFlow(host, flowPath)) {
        qCCritical(lcApp) << "Failed to load flow:" << flowPath;
        return 1;
    }

    qCInfo(lcApp) << "Flow loaded successfully, entering event loop";
    const int result = boot.exec();

    boot.shutdown();
    return result;
}
