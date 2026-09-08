#ifndef DAQSTER_GUI_BUILD_CFG_H
#define DAQSTER_GUI_BUILD_CFG_H

#include <QtCore/qglobal.h>

// When building/using DaqsterGui as a static library, we must not use
// dllimport/dllexport. DAQSTER_GUI_STATIC is defined via CMake in that case.
#if defined(DAQSTER_GUI_STATIC)
#  define DAQSTER_GUI_EXPORT
#elif defined(DAQSTER_GUI_LIBRARY)
#  define DAQSTER_GUI_EXPORT Q_DECL_EXPORT
#else
#  define DAQSTER_GUI_EXPORT Q_DECL_IMPORT
#endif

#endif // DAQSTER_GUI_BUILD_CFG_H