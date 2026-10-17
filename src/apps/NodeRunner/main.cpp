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
 * file only keeps the runner-specific bits: the --headless/--run options and
 * the flow path validation.
 *
 * ── Application object: dlopen()ed launcher (REQ-SW-PL-072) ────────────────
 * Qt's application object is the one thing that must differ between GUI
 * (QApplication, QtWidgets) and headless (QGuiApplication, no QtWidgets). To
 * keep NodeRunner itself free of QtWidgets, the object is created by a small
 * launcher library loaded at runtime:
 *
 *   --headless -> DaqsterLauncherHeadless : QGuiApplication (Qt::Gui only)
 *   GUI mode   -> DaqsterLauncherGui      : QApplication    (QtWidgets)
 *
 * Both launchers create a QGuiApplication-based object and set
 * Qt::AA_ShareOpenGLContexts, so every GPU/GL optimisation works identically in
 * both modes (the pipeline's hardware-GL detection requires a QGuiApplication
 * instance). The module set is therefore a runtime decision, not a link-time one.
 *
 * Note: NodeRunner and the headless launcher do not link QtWidgets, so the
 * runner starts without the widget stack. QPluginManager still loads the
 * remaining plugins afterwards and some of these (GUI runtime host, GUI node
 * widgets, …) do link QtWidgets; keeping those out of a headless run is a
 * separate concern (plugin gating) that the launcher cannot decide.
 *
 * See Daqster::IRuntimeHost (src/plugins/common/capabilities/IRuntimeHost.h).
 */

#include <AppBootstrap.h>

#include <LogCategories.h>
#include <daqster_version.h>

#include <QCommandLineOption>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibrary>
#include <QStandardPaths>
#include <QStringList>

#include <memory>

namespace {

/// Signature of the launcher entry point (DaqsterAppLauncher.h), declared here
/// by hand so NodeRunner needs no header from the launcher library.
using CreateApplicationFn = QCoreApplication* (*)(int, char**);

/// @brief Directories to search for the launcher library, most specific first.
///
/// The launcher sits next to the executable in the build tree (bin/) and in
/// ../lib after installation. A bare library name would NOT find it: glibc
/// honours DT_RUNPATH only when resolving an object's own DT_NEEDED entries,
/// never for dlopen(), so the RUNPATH that lets NodeRunner find its linked
/// libraries does not apply here. Resolve an absolute candidate from the
/// executable's directory instead (QLibrary adds the platform prefix/suffix).
QStringList launcherSearchDirs(const QString& programPath)
{
    QString exePath = programPath;
    const bool hasSeparator = exePath.contains(QLatin1Char('/'))
                              || exePath.contains(QLatin1Char('\\'));
    if (exePath.isEmpty()) {
        return {};
    } else if (!QFileInfo(exePath).isAbsolute() && !hasSeparator) {
        // Bare command name (found via PATH).
        exePath = QStandardPaths::findExecutable(exePath);
        if (exePath.isEmpty())
            return {};
    }
    // Absolute, or relative to the current directory ("build_qt5/bin/NodeRunner").
    const QDir exeDir = QFileInfo(exePath).absoluteDir();
    return QStringList()
        << exeDir.absolutePath()
        << QDir(exeDir.absoluteFilePath(QStringLiteral("../lib"))).absolutePath();
}

} // namespace

int main(int argc, char *argv[])
{
    // ── Platform selection must happen before the app object is built ───────
    // The platform plugin is read at construction, so --headless is parsed from
    // the raw argv by the bootstrap instead of via QCommandLineParser.
    const bool headlessMode = Daqster::AppBootstrap::detectHeadless(argc, argv);
    Daqster::AppBootstrap::preparePlatform(headlessMode);

    // ── Application object through the dlopen()ed launcher ──────────────────
    // The launcher library (Widgets-free for headless, QtWidgets for GUI) lives
    // next to the executable (bin/ in the build tree, lib/ once installed).
    // DT_RUNPATH is not consulted for dlopen(), so the search is done explicitly
    // from argv[0] (see launcherSearchDirs).
    const QString launcherName = headlessMode
        ? QStringLiteral(DAQSTER_LAUNCHER_HEADLESS_NAME)
        : QStringLiteral(DAQSTER_LAUNCHER_GUI_NAME);

    QLibrary launcher;
    for (const QString& dir : launcherSearchDirs(QString::fromLocal8Bit(argv[0]))) {
        launcher.setFileName(QDir(dir).absoluteFilePath(launcherName));
        if (launcher.load())
            break;
    }
    if (!launcher.isLoaded())
        launcher.setFileName(launcherName); // last resort: loader search path

    const auto createApplication = reinterpret_cast<CreateApplicationFn>(
        launcher.resolve("daqster_create_application"));
    if (!createApplication) {
        qCCritical(lcApp) << "Cannot load application launcher" << launcherName
                          << ":" << launcher.errorString();
        return 1;
    }

    std::unique_ptr<QCoreApplication> app(createApplication(argc, argv));
    if (!app) {
        qCCritical(lcApp) << "Application launcher" << launcherName
                          << "failed to create the application object";
        return 1;
    }
    QCoreApplication::setApplicationName(DAQSTER_RUNNER_TARGET_NAME);
    QCoreApplication::setApplicationVersion(DAQSTER_VERSION_STRING);

    Daqster::AppBootstrap boot(
        *app, "NodeRunner - Run a .flow scene, headless or with a visible runtime UI");

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
