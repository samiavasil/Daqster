// DaqsterLauncherGui — application-object launcher for the GUI mode
// (REQ-SW-PL-072).
//
// The only library in the tree that exists to pair QtWidgets with the
// application object: it creates a QApplication. NodeRunner dlopen()s this
// library only when no --headless was requested, so the widget stack enters the
// process exclusively on the GUI path.

#include "DaqsterAppLauncher.h"

#include <QApplication>
#include <QCoreApplication>

// QCoreApplicationPrivate stores `int &argc` — a *reference* to the count the
// application object was constructed with (Qt's qcoreapplication_p.h) and reads
// it again later. Passing this function's own parameter would dangle as soon as
// it returns, so the count given to Qt needs static storage duration (the argv
// array is the caller's and lives for the whole process).
static int s_argc = 0;

extern "C" QCoreApplication* daqster_create_application(int argc, char** argv)
{
    s_argc = argc;

    // Same context-sharing request as the headless launcher: GPU/GL
    // optimisations must behave identically in both modes (REQ-SW-PL-032).
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

    return new QApplication(s_argc, argv);
}
