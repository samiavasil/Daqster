// DaqsterLauncherHeadless — application-object launcher for the headless mode
// (REQ-SW-PL-072).
//
// Deliberately links Qt::Gui only. It creates a QGuiApplication, NOT a
// QCoreApplication: the video pipeline's hardware-GL detection
// (VideoGLContextManager::hasHardwareGL) needs QGuiApplication::instance() to
// bring up the platform integration, so a plain QCoreApplication would make the
// GPU path report "no hardware GL". QGuiApplication is the minimum that keeps
// every GPU/GL optimisation working without pulling in QtWidgets.

#include "DaqsterAppLauncher.h"

#include <QCoreApplication>
#include <QGuiApplication>

// QCoreApplicationPrivate stores `int &argc` — a *reference* to the count the
// application object was constructed with (see Qt's qcoreapplication_p.h), and
// it reads that reference again later (e.g. QCoreApplication::arguments()).
// Passing this function's own parameter would leave the reference dangling the
// instant the function returns, so the count handed to Qt must have static
// storage duration. The argv array is the caller's (main's) and lives for the
// whole process, so only the count needs keeping.
static int s_argc = 0;

extern "C" QCoreApplication* daqster_create_application(int argc, char** argv)
{
    s_argc = argc;

    // Context sharing must be requested before the application object exists so
    // that the shared GPU context and any QOpenGLWidget live in one share group
    // (REQ-SW-PL-032). Same setting the GUI path uses — GPU/GL parity between
    // the two modes.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

    return new QGuiApplication(s_argc, argv);
}
