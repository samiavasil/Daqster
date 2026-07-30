#include "ShutdownHandler.h"

#ifdef Q_OS_WIN
#include "WindowsShutdownHandler.h"
#else
#include "UnixShutdownHandler.h"
#endif

ShutdownHandler* ShutdownHandler::create(QObject *parent)
{
#ifdef Q_OS_WIN
    return new WindowsShutdownHandler(parent);
#else
    return new UnixShutdownHandler(parent);
#endif
}
