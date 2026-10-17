#ifndef DAQSTER_APP_LAUNCHER_H
#define DAQSTER_APP_LAUNCHER_H

/**
 * @brief C entry point shared by the application-object launcher libraries.
 *
 * NodeRunner must not link QtWidgets (REQ-SW-PL-072): the headless process has
 * to stay widget-free so it never loads the widget stack nor the GL/QPA bits
 * that only the GUI path needs. The one remaining QtWidgets user is the
 * application object itself, so it is factored out of the executable into two
 * tiny dlopen()ed launcher libraries that export the same C symbol:
 *
 *   DaqsterLauncherHeadless - Qt::Gui              -> QGuiApplication
 *   DaqsterLauncherGui      - Qt::Gui + Qt::Widgets -> QApplication
 *
 * NodeRunner selects the library for the requested mode and dlopen()s exactly
 * one of them, so the set of Qt modules linked into the process is a runtime
 * decision, not a link-time one. Both launchers create a QGuiApplication-based
 * object on purpose: the video pipeline's hardware-GL detection
 * (VideoGLContextManager::hasHardwareGL) requires QGuiApplication::instance(),
 * so a plain QCoreApplication would silently disable the GPU path.
 *
 * The declaration is exported with C linkage so QLibrary::resolve() can find it
 * by name without C++ name mangling.
 */

class QCoreApplication;

#if defined(_WIN32) || defined(__CYGWIN__)
#  define DAQSTER_APP_LAUNCHER_EXPORT __declspec(dllexport)
#else
#  define DAQSTER_APP_LAUNCHER_EXPORT __attribute__((visibility("default")))
#endif

/// Signature of daqster_create_application(); resolved through QLibrary.
typedef QCoreApplication* (*DaqsterCreateApplicationFn)(int argc, char** argv);

extern "C" DAQSTER_APP_LAUNCHER_EXPORT QCoreApplication*
daqster_create_application(int argc, char** argv);

#endif // DAQSTER_APP_LAUNCHER_H
