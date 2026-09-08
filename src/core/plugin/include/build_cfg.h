#ifndef DAQSTER_CORE_BUILD_CFG_H
#define DAQSTER_CORE_BUILD_CFG_H

#include <QtCore/qglobal.h>

// When building/using DaqsterCore as a static library, we must not use
// dllimport/dllexport. DAQSTER_CORE_STATIC is defined via CMake in that case.
#if defined(DAQSTER_CORE_STATIC)
#  define DAQSTER_CORE_EXPORT
#elif defined(DAQSTER_CORE_LIBRARY)
#  define DAQSTER_CORE_EXPORT Q_DECL_EXPORT
#else
#  define DAQSTER_CORE_EXPORT Q_DECL_IMPORT
#endif

#endif // DAQSTER_CORE_BUILD_CFG_H